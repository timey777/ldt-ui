// Clean, rule-only implementation for LayoutEngine.
// LayoutEngine parses @layout blocks and exposes query APIs similar to StyleEngine.

#include "layout_engine.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include "engine/core/ast_node.h"
#include "engine/core/node_flags.h"
#include "engine/core/resolved_node.h"
#include "engine/box_model_engine.h"
#include "misc/logger.h"

namespace {

std::string trimCopy(const std::string& value) {
    size_t start = 0;
    size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::vector<std::string> splitSelectors(const std::string& selector) {
    std::vector<std::string> selectors;
    std::stringstream stream(selector);
    std::string token;
    while (std::getline(stream, token, ',')) {
        auto trimmed = trimCopy(token);
        if (!trimmed.empty()) selectors.push_back(trimmed);
    }
    return selectors;
}

std::optional<std::string> attributeToString(const Attribute& attr) {
    try {
        if (attr.isString()) return attr.as<std::string>();
        if (attr.isInt()) return std::to_string(attr.as<int>());
        if (attr.isFloat()) return std::to_string(attr.as<float>());
        if (attr.isBool()) return attr.as<bool>() ? std::string("true") : std::string("false");
    } catch (...) {
        LDT_ERROR("LayoutEngine::attributeToString failed");
    }
    return std::nullopt;
}

void applyLayoutProperty(ldt::CompiledLayoutRules& out, const std::string& key, const Attribute& attr) {
    auto value = attributeToString(attr);
    if (!value.has_value()) return;

    try {
        if (key == "display") {
            out.setDisplay(ldt::displayStringToEnum(*value));
            return;
        }
        if (key == "position") {
            out.position = ldt::positionStringToEnum(*value);
            return;
        }
        if (key == "flex-direction") {
            out.flexDirection = ldt::flexDirectionStringToEnum(*value);
            return;
        }
        if (key == "align-items") {
            out.alignItems = ldt::alignItemsStringToEnum(*value);
            return;
        }
        if (key == "justify-content") {
            out.justifyContent = ldt::justifyContentStringToEnum(*value);
            return;
        }
        if (key == "flex-wrap") {
            out.flexWrap = ldt::flexWrapStringToEnum(*value);
            return;
        }
        if (key == "flex-grow") {
            out.flexGrow = std::stof(*value);
            return;
        }
        if (key == "flex-shrink") {
            out.flexShrink = std::stof(*value);
            return;
        }
        if (key == "gap") {
            float parsed = 0.0f;
            if (ldt::tryParseAbsoluteLength(*value, parsed)) {
                out.gap = parsed;
            }
            return;
        }
        if (key == "grid-template-columns") {
            out.gridTemplateColumns = *value;
            return;
        }
        if (key == "grid-template-rows") {
            out.gridTemplateRows = *value;
            return;
        }
    } catch (...) {
        return;
    }
}

bool isLayoutCompileProperty(const std::string& key) {
    return key == "display" ||
           key == "position" ||
           key == "flex-direction" ||
           key == "align-items" ||
           key == "justify-content" ||
           key == "flex-wrap" ||
           key == "flex-grow" ||
           key == "flex-shrink" ||
           key == "gap" ||
           key == "grid-template-columns" ||
           key == "grid-template-rows";
}

} // namespace

std::string LayoutEngine::name() const { return "LayoutEngine"; }

LayoutEngine::LayoutEngine() = default;
LayoutEngine::~LayoutEngine() = default;

void LayoutEngine::initialize() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
    loadDefaultLayoutRules();
}

void LayoutEngine::shutdown() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
    lastAST_.reset();
}

void LayoutEngine::reset() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
    loadDefaultLayoutRules();
}

ldt::CompiledLayoutRules LayoutEngine::compile(std::shared_ptr<ASTNode> ast) const
{
    ldt::CompiledLayoutRules out;
    if (!ast) return out;

    auto applyRule = [&](const LayoutRule& rule) {
        if (!matchesSelector(ast, rule.selector)) return;
        for (const auto& [key, value] : rule.properties) {
            applyLayoutProperty(out, key, value);
        }
    };

    auto applyBucket = [&](const LayoutRuleBucket& bucket) {
        for (const auto& rule : bucket.rules) {
            applyRule(rule);
        }
    };

    applyBucket(defaultBucket_);
    applyBucket(globalBucket_);

    const std::optional<int> ownerId = ast->getLocalScopeId();
    if (ownerId.has_value()) {
        auto localIt = localBuckets_.find(ownerId.value());
        if (localIt != localBuckets_.end()) {
            applyBucket(localIt->second);
        }
    }

    for (const auto& [key, value] : ast->attrs) {
        if (!isLayoutCompileProperty(key)) continue;
        applyLayoutProperty(out, key, value);
    }

    return out;
}

std::type_index LayoutEngine::type() const
{
    return typeid(LayoutEngine);
}

void LayoutEngine::process(std::shared_ptr<ASTNode> root, ldt::DisplayList& outList,
                           float viewportW, float viewportH) {
    // LayoutEngine only parses @layout blocks and builds layout rules.
    // It does not produce a DisplayList nor compute box-models.

    // 优先查找 parser 在解析阶段分离出来的 meta 存储（例如 root->meta["@layout"]）
    globalBucket_.clear();

    if (root) {
        if (auto meta = root->getMeta("@layout")) {
            for (const auto& node : *meta) parseLayoutRules(node, globalBucket_);
        }
    }
    lastAST_ = root;
}

void LayoutEngine::onASTUpdated(std::shared_ptr<ASTNode> root) {
    if (!root) return;

    globalBucket_.clear();
    if (auto meta = root->getMeta("@layout")) {
        for (const auto& node : *meta) parseLayoutRules(node, globalBucket_);
    }
    lastAST_ = root;
}

void LayoutEngine::setScopedRules(std::shared_ptr<ASTNode> root, LayoutRuleScope scope,
                                  std::optional<int> ownerId, bool replace) {
    if (!root) return;

    LayoutRuleBucket* target = nullptr;
    if (scope == LayoutRuleScope::Default) {
        target = &defaultBucket_;
    } else if (scope == LayoutRuleScope::Global) {
        target = &globalBucket_;
    } else {
        if (!ownerId.has_value()) return;
        target = &localBuckets_[ownerId.value()];
    }

    if (replace && target) {
        target->clear();
    }

    if (auto meta = root->getMeta("@layout")) {
        for (const auto& node : *meta) parseLayoutRules(node, *target);
    }
}

void LayoutEngine::clearScopedRules(LayoutRuleScope scope, std::optional<int> ownerId) {
    if (scope == LayoutRuleScope::Default) {
        defaultBucket_.clear();
        return;
    }
    if (scope == LayoutRuleScope::Global) {
        globalBucket_.clear();
        return;
    }
    if (ownerId.has_value()) {
        localBuckets_.erase(ownerId.value());
    }
}

void LayoutEngine::parseLayoutRules(std::shared_ptr<ASTNode> node, LayoutRuleBucket& bucket) {
    if (!node) return;

    // Find @layout blocks (node types may be "Layout" or "LayoutBlock")
    if (node->type == "Layout" || node->type == "LayoutBlock") {
        for (const auto& child : node->children) {
            parseLayoutRule(child, bucket);
        }
    }

    for (const auto& child : node->children) {
        parseLayoutRules(child, bucket);
    }
}

void LayoutEngine::parseLayoutRule(std::shared_ptr<ASTNode> ruleNode, LayoutRuleBucket& bucket) {
    if (!ruleNode) return;

    // Selector can be provided as attribute "selector" or inferred from node type
    std::string selector;
    if (auto sel = ruleNode->getAttribute("selector")) {
        try {
            if (sel->isString()) selector = sel->as<std::string>();
        } catch (...) {
            LDT_ERROR("LayoutEngine::parseLayoutRule: failed to read selector");
        }
    }
    if (selector.empty()) selector = ruleNode->type;
    if (selector.empty()) return;

    auto selectors = splitSelectors(selector);
    if (selectors.empty()) return;

    std::map<std::string, Attribute> properties;
    for (const auto& attrPair : ruleNode->attrs) {
        const std::string& key = attrPair.first;
        const Attribute& attr = attrPair.second;
        if (key == "selector") continue;
        properties[key] = attr;
    }

    for (const auto& currentSelector : selectors) {
        LayoutRule rule;
        rule.selector = currentSelector;
        rule.properties = properties;
        bucket.rules.push_back(rule);
    }
}

const Attribute* LayoutEngine::getLayoutProperty(std::shared_ptr<ASTNode> node, const std::string& property) const {
    if (!node) return nullptr;

    // Inline attribute on node has highest priority
    if (auto attr = node->getAttribute(property)) return attr;

    auto findInBucket = [&](const LayoutRuleBucket& bucket) -> const Attribute* {
        for (auto it = bucket.rules.rbegin(); it != bucket.rules.rend(); ++it) {
            if (!matchesSelector(node, it->selector)) continue;
            auto propIt = it->properties.find(property);
            if (propIt != it->properties.end()) return &propIt->second;
        }
        return nullptr;
    };

    const std::optional<int> ownerId = node->getLocalScopeId();
    if (ownerId.has_value()) {
        auto localIt = localBuckets_.find(ownerId.value());
        if (localIt != localBuckets_.end()) {
            if (auto attr = findInBucket(localIt->second)) return attr;
        }
    }

    if (auto attr = findInBucket(globalBucket_)) return attr;
    if (auto attr = findInBucket(defaultBucket_)) return attr;

    return nullptr;
}

std::string LayoutEngine::getLayoutPropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                                  const std::string& defaultValue) const {
    auto attr = getLayoutProperty(node, property);
    if (!attr) return defaultValue;
    try {
        if (attr->isString()) return attr->as<std::string>();
        if (attr->isInt()) return std::to_string(attr->as<int>());
        if (attr->isFloat()) return std::to_string(attr->as<float>());
    } catch (...) {
        LDT_ERROR("LayoutEngine::getLayoutPropertyString failed for property=" << property);
    }
    return defaultValue;
}

bool LayoutEngine::hasLayoutProperty(std::shared_ptr<ASTNode> node, const std::string& property) const {
    return getLayoutProperty(node, property) != nullptr;
}

bool LayoutEngine::matchesSelector(std::shared_ptr<ASTNode> node, const std::string& selector) const {
    if (!node || selector.empty()) return false;

    if (selector == "*") return true;

    // Support #id, .class and type selectors (simple matching like StyleEngine)
    if (selector[0] == '#') {
        std::string id = selector.substr(1);
        if (auto idAttr = node->getAttribute("id")) {
            try { return idAttr->isString() && idAttr->as<std::string>() == id; } catch(...) {}
        }
        return false;
    } else if (selector[0] == '.') {
        std::string className = selector.substr(1);
        // Use cached classList if available
        if (!node->getClassList().empty()) {
            const auto& list = node->getClassList();
            for (const auto& cls : list) {
                if (cls == className) return true;
            }
            return false;
        }
        return false;
    } else {
        return node->type == selector;
    }
}

void LayoutEngine::clearLayoutRules() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
}

void LayoutEngine::loadDefaultLayoutRules() {
    defaultBucket_.clear();

    addDefaultLayoutRule("panel", {
        {"display", Attribute("block")},
        {"position", Attribute("static")}
    });

    addDefaultLayoutRule("button", {
        {"display", Attribute("inline-block")},
        {"position", Attribute("static")}
    });

    addDefaultLayoutRule("text", {
        {"display", Attribute("inline")},
    });

    // Flex helper class
    addDefaultLayoutRule(".flex", {
        {"display", Attribute("flex")},
        {"flex-direction", Attribute("row")},
        {"flex-wrap", Attribute("nowrap")}
    });

    // Grid helper class
    addDefaultLayoutRule(".grid", {
        {"display", Attribute("grid")}
    });
}

void LayoutEngine::addDefaultLayoutRule(const std::string& selector, const std::map<std::string, Attribute>& properties) {
    LayoutRule rule;
    rule.selector = selector;
    rule.properties = properties;
    defaultBucket_.rules.push_back(rule);
}


