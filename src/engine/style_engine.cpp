#include "style_engine.h"
#include "document_runtime.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include "engine/core/ast_node.h"
#include "engine/core/resolved_node.h"
#include "misc/logger.h"

using namespace ldt;

std::string StyleEngine::name() const { return "StyleEngine"; }

// Helper: parse color string via display_list Color parser
static ldt::ui::Color parseColorAttr(const Attribute* a) {
    if (!a) return ldt::ui::Color();
    try {
        if (a->isString()) {
            return ldt::ui::Color::fromHex(a->as<std::string>());
        }
    } catch (...) {
        LDT_ERROR("StyleEngine::parseColorAttr failed to parse color");
    }
    return ldt::ui::Color();
}

// Helper: parse border shorthand string (e.g. "1px solid #000")
static void parseBorderShorthand(const std::string &s, float &outWidth, ldt::ui::Color &outColor) {
    if (s == "none") {
        outWidth = 0.0f;
        return;
    }
    outWidth = -1.0f;
    std::stringstream ss(s);
    std::string tok;
    while (ss >> tok) {
        if (!tok.empty() && (tok.back() == ';' || tok.back() == ',')) tok.pop_back();
        float v = 0.0f;
        if (ldt::tryParseAbsoluteLength(tok, v)) {
            if (outWidth < 0.0f) outWidth = v;
            continue;
        }

        if (!tok.empty() && tok[0] == '#') {
            try {
                outColor = ldt::ui::Color::fromHex(tok);
                continue;
            } catch (...) {
                LDT_ERROR("StyleEngine::parseBorderShorthand failed to parse color");
            }
        }
    }
    if (outWidth < 0.0f) outWidth = 0.0f;
}

// Helper: parse edge values (margin/padding) handling "auto" and "px"
static void parseEdgeValues(const std::string& s, ui::Edges& out, std::array<bool, 4>* outAuto = nullptr) {
    (void)ldt::tryParseEdgeLengthShorthand(s, out, outAuto);
}

static bool tryParseAbsoluteLengthAttr(const Attribute& attr, float& outValue) {
    if (attr.isFloat()) {
        outValue = attr.as<float>();
        return true;
    }
    if (attr.isInt()) {
        outValue = static_cast<float>(attr.as<int>());
        return true;
    }
    if (attr.isString()) {
        return ldt::tryParseAbsoluteLength(attr.as<std::string>(), outValue);
    }
    return false;
}

static bool tryParseLayoutUnitAttr(const Attribute& attr, ldt::LayoutUnit& outUnit) {
    if (attr.isString()) {
        outUnit = ldt::LayoutUnit::fromString(attr.as<std::string>());
        return true;
    }
    if (attr.isInt()) {
        outUnit = ldt::LayoutUnit::px(static_cast<float>(attr.as<int>()));
        return true;
    }
    if (attr.isFloat()) {
        outUnit = ldt::LayoutUnit::px(attr.as<float>());
        return true;
    }
    return false;
}

static bool tryParseUIUnitAttr(const Attribute& attr, ldt::UIUnit& outUnit) {
    if (attr.isString()) {
        outUnit = ldt::UIUnit::fromString(attr.as<std::string>());
        return true;
    }
    if (attr.isFloat()) {
        outUnit = ldt::UIUnit::px(attr.as<float>());
        return true;
    }
    if (attr.isInt()) {
        outUnit = ldt::UIUnit::px(static_cast<float>(attr.as<int>()));
        return true;
    }
    return false;
}

static bool tryParseEdgesAttr(const Attribute& attr, ui::Edges& outEdges,
                              std::array<bool, 4>* outAuto = nullptr) {
    if (!attr.isString()) return false;
    parseEdgeValues(attr.as<std::string>(), outEdges, outAuto);
    return true;
}

static bool tryParseEdgeSideAttr(const Attribute& attr, int sideIndex, float& outValue,
                                 bool* outAuto = nullptr) {
    if (attr.isString()) {
        ui::Edges edges;
        std::array<bool, 4> autoFlags = {false, false, false, false};
        parseEdgeValues(attr.as<std::string>(), edges, outAuto ? &autoFlags : nullptr);
        outValue = edges[sideIndex];
        if (outAuto) {
            *outAuto = autoFlags[sideIndex];
        }
        return true;
    }

    if (tryParseAbsoluteLengthAttr(attr, outValue)) {
        if (outAuto) {
            *outAuto = false;
        }
        return true;
    }

    return false;
}

static bool applyLengthField(float& target, const Attribute& attr) {
    float parsed = 0.0f;
    if (!tryParseAbsoluteLengthAttr(attr, parsed)) {
        return false;
    }
    target = parsed;
    return true;
}

static bool applyLayoutUnitField(ldt::LayoutUnit& target, const Attribute& attr) {
    ldt::LayoutUnit parsed;
    if (!tryParseLayoutUnitAttr(attr, parsed)) {
        return false;
    }
    target = parsed;
    return true;
}

static bool applyUIUnitField(ldt::UIUnit& target, const Attribute& attr) {
    ldt::UIUnit parsed;
    if (!tryParseUIUnitAttr(attr, parsed)) {
        return false;
    }
    target = parsed;
    return true;
}

static bool applyEdgeGroup(ui::Edges& target, const Attribute& attr) {
    if (attr.isString()) {
        const std::string value = attr.as<std::string>();
        if (value == "none") {
            target = ui::Edges(0.0f);
            return true;
        }
    }

    if (tryParseEdgesAttr(attr, target)) {
        return true;
    }

    float parsed = 0.0f;
    if (!tryParseAbsoluteLengthAttr(attr, parsed)) {
        return false;
    }
    target = ui::Edges(parsed);
    return true;
}

static bool applyEdgeSide(ui::Edges& target, int sideIndex, const Attribute& attr) {
    float parsed = 0.0f;
    if (!tryParseEdgeSideAttr(attr, sideIndex, parsed)) {
        return false;
    }
    target[sideIndex] = parsed;
    return true;
}

static bool applyMarginGroup(ui::Edges& target, std::array<bool, 4>& targetAuto, const Attribute& attr) {
    return tryParseEdgesAttr(attr, target, &targetAuto);
}

static bool applyMarginSide(ui::Edges& target, std::array<bool, 4>& targetAuto,
                            int sideIndex, const Attribute& attr) {
    float parsed = 0.0f;
    bool isAuto = false;
    if (!tryParseEdgeSideAttr(attr, sideIndex, parsed, &isAuto)) {
        return false;
    }
    target[sideIndex] = parsed;
    targetAuto[sideIndex] = isAuto;
    return true;
}

static bool ensureDeltaProp(ldt::StyleDelta& out, ldt::StyleProp prop, const ui::Edges& defaultValue = ui::Edges()) {
    if (out.has(prop)) {
        return false;
    }

    switch (prop) {
        case ldt::StyleProp::BorderWidth:
            out.borderWidth = defaultValue;
            break;
        case ldt::StyleProp::Padding:
            out.padding = defaultValue;
            break;
        case ldt::StyleProp::Margin:
            out.margin = defaultValue;
            break;
        default:
            break;
    }

    out.set(prop);
    return true;
}

static void ensureMarginDelta(ldt::StyleDelta& out) {
    (void)ensureDeltaProp(out, ldt::StyleProp::Margin);
    if (!out.has(ldt::StyleProp::MarginAuto)) {
        out.marginAuto = std::array<bool, 4>{false, false, false, false};
        out.set(ldt::StyleProp::MarginAuto);
    }
}

// Apply rule property map into ComputedStyle (overwrite fields)
static void applyPropertiesToComputedStyle(ldt::ComputedStyle &out, const std::map<std::string, Attribute>& props) {
    for (const auto &kv : props) {
        const std::string &k = kv.first;
        const Attribute &v = kv.second;
        try {
            if (k == "background-color") out.backgroundColor = parseColorAttr(&v);
            else if (k == "color") out.textColor = parseColorAttr(&v);
            else if (k == "border-color") out.borderColor = parseColorAttr(&v);
            else if (k == "border-width") { 
                (void)applyEdgeGroup(out.borderWidth, v);
            }
            else if (k == "border-width-top") { 
                (void)applyEdgeSide(out.borderWidth, 0, v);
            }
            else if (k == "border-width-right") { 
                (void)applyEdgeSide(out.borderWidth, 1, v);
            }
            else if (k == "border-width-bottom") { 
                (void)applyEdgeSide(out.borderWidth, 2, v);
            }
            else if (k == "border-width-left") { 
                (void)applyEdgeSide(out.borderWidth, 3, v);
            }
            else if (k == "border-radius") {
                (void)applyUIUnitField(out.borderRadius, v);
            }
            else if (k == "border") {
                if (v.isString()) {
                    std::string s = v.as<std::string>();
                    float w = 0.0f; ldt::ui::Color c = out.borderColor;
                    parseBorderShorthand(s, w, c);
                    out.borderWidth = ui::Edges(w);
                    out.borderColor = c;
                } else {
                    (void)applyEdgeGroup(out.borderWidth, v);
                }
            }
            else if (k == "font-size") {
                (void)applyLengthField(out.fontSize, v);
            }
            else if (k == "line-height") {
                (void)applyLengthField(out.lineHeight, v);
            }
            else if (k == "font-family") { if (v.isString()) out.fontFamily = v.as<std::string>(); }
            else if (k == "font-weight") { if (v.isString()) out.fontWeight = ldt::fontWeightFromString(v.as<std::string>()); }
            else if (k == "text-align") { if (v.isString()) out.textAlign = ldt::textAlignFromString(v.as<std::string>()); }
            else if (k == "width") {
                (void)applyLayoutUnitField(out.width, v);
            }
            else if (k == "height") {
                (void)applyLayoutUnitField(out.height, v);
            }
            else if (k == "overflow") { if (v.isString()) out.overflow = ldt::overflowFromString(v.as<std::string>()); }
            else if (k == "box-sizing") { if (v.isString()) out.boxSizing = ldt::boxSizingFromString(v.as<std::string>()); }
            else if (k == "padding") {
                (void)applyEdgeGroup(out.padding, v);
            }
            else if (k == "padding-top") { 
                (void)applyEdgeSide(out.padding, 0, v);
            }
            else if (k == "padding-right") { 
                (void)applyEdgeSide(out.padding, 1, v);
            }
            else if (k == "padding-bottom") { 
                (void)applyEdgeSide(out.padding, 2, v);
            }
            else if (k == "padding-left") { 
                (void)applyEdgeSide(out.padding, 3, v);
            }
            else if (k == "margin") {
                (void)applyMarginGroup(out.margin, out.marginAuto, v);
            }
            else if (k == "margin-top") { 
                (void)applyMarginSide(out.margin, out.marginAuto, 0, v);
            }
            else if (k == "margin-right") { 
                (void)applyMarginSide(out.margin, out.marginAuto, 1, v);
            }
            else if (k == "margin-bottom") { 
                (void)applyMarginSide(out.margin, out.marginAuto, 2, v);
            }
            else if (k == "margin-left") { 
                (void)applyMarginSide(out.margin, out.marginAuto, 3, v);
            }
            else if (k == "opacity") { if (v.isFloat()) out.opacity = v.as<float>(); else if (v.isInt()) out.opacity = static_cast<float>(v.as<int>()); }
            else if (k == "visible") { if (v.isBool()) out.visible = v.as<bool>(); }
            else if (k == "box-shadow") { if (v.isString()) out.boxShadow = v.as<std::string>(); }
            else if (k == "background-image") { if (v.isString()) out.backgroundImage = v.as<std::string>(); }
            else if (k == "overlay") {
                if (v.isBool())
                {
                    auto l = v.as<bool>();;
                    out.overlay = v.as<bool>();
                }
            }
            else if (k == "anchor") { if (v.isString()) out.anchor = stringToAnchor(v.as<std::string>()); }
        } catch (...) {
            LDT_ERROR("StyleEngine::applyPropertiesToComputedStyle failed for key=" << k);
        }
    }
}

// Apply properties into StyleDelta (optionals)
static void applyPropertiesToStyleDelta(ldt::StyleDelta &out, const std::map<std::string, Attribute>& props) {
    for (const auto &kv : props) {
        const std::string &k = kv.first;
        const Attribute &v = kv.second;
        try {
            if (k == "background-color") { out.backgroundColor = parseColorAttr(&v); out.set(ldt::StyleProp::BackgroundColor); }
            else if (k == "color") { out.textColor = parseColorAttr(&v); out.set(ldt::StyleProp::TextColor); }
            else if (k == "border-color") { out.borderColor = parseColorAttr(&v); out.set(ldt::StyleProp::BorderColor); }
            else if (k == "border-width") { 
                if (applyEdgeGroup(out.borderWidth, v)) out.set(ldt::StyleProp::BorderWidth);
            }
            else if (k == "border-width-top") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::BorderWidth);
                (void)applyEdgeSide(out.borderWidth, 0, v);
            }
            else if (k == "border-width-right") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::BorderWidth);
                (void)applyEdgeSide(out.borderWidth, 1, v);
            }
            else if (k == "border-width-bottom") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::BorderWidth);
                (void)applyEdgeSide(out.borderWidth, 2, v);
            }
            else if (k == "border-width-left") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::BorderWidth);
                (void)applyEdgeSide(out.borderWidth, 3, v);
            }
            else if (k == "border-radius") { 
                if (applyUIUnitField(out.borderRadius, v)) out.set(ldt::StyleProp::BorderRadius);
            }
            else if (k == "border") {
                if (v.isString()) {
                    std::string s = v.as<std::string>();
                    float w = 0.0f; ldt::ui::Color c = out.has(ldt::StyleProp::BorderColor) ? out.borderColor : ldt::ui::Color();
                    parseBorderShorthand(s, w, c);
                    out.borderWidth = ui::Edges(w); out.set(ldt::StyleProp::BorderWidth);
                    out.borderColor = c; out.set(ldt::StyleProp::BorderColor);
                } else {
                    if (applyEdgeGroup(out.borderWidth, v)) out.set(ldt::StyleProp::BorderWidth);
                }
            }
            else if (k == "font-size") { 
                if (applyLengthField(out.fontSize, v)) out.set(ldt::StyleProp::FontSize);
            }
            else if (k == "line-height") { 
                if (applyLengthField(out.lineHeight, v)) out.set(ldt::StyleProp::LineHeight);
            }
            else if (k == "font-family") { if (v.isString()) { out.fontFamily = v.as<std::string>(); out.set(ldt::StyleProp::FontFamily); } }
            else if (k == "font-weight") { if (v.isString()) { out.fontWeight = ldt::fontWeightFromString(v.as<std::string>()); out.set(ldt::StyleProp::FontWeight); } }
            else if (k == "text-align") { if (v.isString()) { out.textAlign = ldt::textAlignFromString(v.as<std::string>()); out.set(ldt::StyleProp::TextAlign); } }
            else if (k == "width") {
                if (applyLayoutUnitField(out.width, v)) out.set(ldt::StyleProp::Width);
            }
            else if (k == "height") {
                if (applyLayoutUnitField(out.height, v)) out.set(ldt::StyleProp::Height);
            }
            else if (k == "overflow") { if (v.isString()) { out.overflow = ldt::overflowFromString(v.as<std::string>()); out.set(ldt::StyleProp::Overflow); } }
            else if (k == "box-sizing") { if (v.isString()) { out.boxSizing = ldt::boxSizingFromString(v.as<std::string>()); out.set(ldt::StyleProp::BoxSizing); } }
            else if (k == "padding") {
                if (applyEdgeGroup(out.padding, v)) {
                    out.set(ldt::StyleProp::Padding);
                }
            }
            else if (k == "padding-top") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::Padding);
                (void)applyEdgeSide(out.padding, 0, v);
            }
            else if (k == "padding-right") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::Padding);
                (void)applyEdgeSide(out.padding, 1, v);
            }
            else if (k == "padding-bottom") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::Padding);
                (void)applyEdgeSide(out.padding, 2, v);
            }
            else if (k == "padding-left") { 
                (void)ensureDeltaProp(out, ldt::StyleProp::Padding);
                (void)applyEdgeSide(out.padding, 3, v);
            }
            else if (k == "margin") {
                if (applyMarginGroup(out.margin, out.marginAuto, v)) {
                    out.set(ldt::StyleProp::Margin);
                    out.set(ldt::StyleProp::MarginAuto);
                }
            }
            else if (k == "margin-top") { 
                ensureMarginDelta(out);
                (void)applyMarginSide(out.margin, out.marginAuto, 0, v);
            }
            else if (k == "margin-right") { 
                ensureMarginDelta(out);
                (void)applyMarginSide(out.margin, out.marginAuto, 1, v);
            }
            else if (k == "margin-bottom") { 
                ensureMarginDelta(out);
                (void)applyMarginSide(out.margin, out.marginAuto, 2, v);
            }
            else if (k == "margin-left") { 
                ensureMarginDelta(out);
                (void)applyMarginSide(out.margin, out.marginAuto, 3, v);
            }
            else if (k == "opacity") { 
                if (v.isFloat()) { out.opacity = v.as<float>(); out.set(ldt::StyleProp::Opacity); } 
                else if (v.isInt()) { out.opacity = static_cast<float>(v.as<int>()); out.set(ldt::StyleProp::Opacity); } 
            }
            else if (k == "visible") { if (v.isBool()) { out.visible = v.as<bool>(); out.set(ldt::StyleProp::Visible); } }
            else if (k == "box-shadow") { if (v.isString()) { out.boxShadow = v.as<std::string>(); out.set(ldt::StyleProp::BoxShadow); } }
            else if (k == "background-image") { if (v.isString()) { out.backgroundImage = v.as<std::string>(); out.set(ldt::StyleProp::BackgroundImage); } }
        } catch (...) {
            LDT_ERROR("StyleEngine::applyPropertiesToStyleDelta failed for key=" << k);
        }
    }
}

// 判断属性名是否为布局相关属性
static bool isLayoutProperty(std::string_view name) {
    static const std::unordered_set<std::string> layoutProps = {
        "width", "height",
        "margin", "padding", "border-width",
        "display",
        "flex-direction",
        "justify-content",
        "align-items"
    };
    return layoutProps.count(std::string(name)) > 0 ||
           name.find("margin-") == 0 || name.find("padding-") == 0 || name.find("border-width-") == 0;
}

// 判断属性是否可继承（CSS 规范）
static bool isInheritableProperty(std::string_view name) {
    static const std::unordered_set<std::string> inheritableProps = {
        "color",
        "font-size",
        "font-family",
        "font-weight",
        "line-height",
        "text-align",
        "visible"
    };
    return inheritableProps.count(std::string(name)) > 0;
}

void StyleEngine::initialize() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
    loadDefaultStyles();
}

void StyleEngine::shutdown() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
}

std::type_index StyleEngine::type() const
{
    return typeid(StyleEngine);
}

void StyleEngine::reset() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
    loadDefaultStyles();
}

void StyleEngine::setScopedRules(std::shared_ptr<ASTNode> root, StyleRuleScope scope,
                                 std::optional<int> ownerId, bool replace) {
    if (!root) return;

    StyleRuleBucket* target = nullptr;
    if (scope == StyleRuleScope::Default) {
        target = &defaultBucket_;
    } else if (scope == StyleRuleScope::Global) {
        target = &globalBucket_;
    } else {
        if (!ownerId.has_value()) return;
        target = &localBuckets_[ownerId.value()];
    }

    if (replace && target) {
        target->clear();
    }

    if (auto meta = root->getMeta("@style")) {
        for (const auto& node : *meta) parseStyleRules(node, *target);
    }

    rebuildBucketIndex(*target);
}

void StyleEngine::clearScopedRules(StyleRuleScope scope, std::optional<int> ownerId) {
    if (scope == StyleRuleScope::Default) {
        defaultBucket_.clear();
        return;
    }
    if (scope == StyleRuleScope::Global) {
        globalBucket_.clear();
        return;
    }
    if (ownerId.has_value()) {
        localBuckets_.erase(ownerId.value());
    }
}

// Helper to extract pseudo state from selector
static void extractPseudoState(const std::string& rawSelector, std::string& outSelector, UIState& outState) {
    outSelector = rawSelector;
    outState = UIState::None;
    
    // Trim trailing spaces initially
    while (!outSelector.empty() && isspace((unsigned char)outSelector.back())) outSelector.pop_back();

    auto checkAndStrip = [&](const std::string& suffix, UIState state) -> bool {
        if (outSelector.size() >= suffix.size()) {
            // Check if it ends with suffix
            if (outSelector.compare(outSelector.size() - suffix.size(), suffix.size(), suffix) == 0) {
                outSelector = outSelector.substr(0, outSelector.size() - suffix.size());
                // Trim spaces again after stripping
                while (!outSelector.empty() && isspace((unsigned char)outSelector.back())) outSelector.pop_back();
                outState = state;
                return true;
            }
        }
        return false;
    };

    if (checkAndStrip(":hover", UIState::Hover)) return;
    if (checkAndStrip(":active", UIState::Active)) return;
    if (checkAndStrip(":focus", UIState::Focus)) return;
    if (checkAndStrip(":disabled", UIState::Disabled)) return;
}

void StyleEngine::process(std::shared_ptr<ASTNode> root, ldt::DisplayList& outList,
                         float viewportW, float viewportH) {
    (void)outList;
    (void)viewportW;
    (void)viewportH;

    globalBucket_.clear();

    // StyleEngine 只负责解析 @style 块
    // 优先查找 parser 在解析阶段分离出来的 meta 存储（例如 root->meta["@style"]）
    if (root) {
        // 尝试用 "@style" 关键字查找（这是 TokenStream 里关键词原样）
        if (auto meta = root->getMeta("@style")) {
            for (const auto& node : *meta) parseStyleRules(node, globalBucket_);
        }
    }
    rebuildBucketIndex(globalBucket_);
}

void StyleEngine::onASTUpdated(std::shared_ptr<ASTNode> root) {
    // 当 AST 被外部更新（hot-reload 或运行时修改）时，重新解析 @style 并通知宿主重建 DisplayList
    if (!root) return;

    // AST 更新仅刷新全局规则，本地规则保持不变
    globalBucket_.clear();

    if (auto meta = root->getMeta("@style")) {
        for (const auto& node : *meta) parseStyleRules(node, globalBucket_);
    }
    rebuildBucketIndex(globalBucket_);
}

void StyleEngine::parseStyleRules(std::shared_ptr<ASTNode> node, StyleRuleBucket& bucket) {
    if (!node) return;
    
    // 查找 @style 节点
    if (node->type == "Style" || node->type == "StyleBlock") {
        // 遍历样式规则
        for (const auto& child : node->children) {
            parseStyleRule(child, bucket);
        }
    }
    
    // 递归处理子节点
    for (const auto& child : node->children) {
        parseStyleRules(child, bucket);
    }
}

void StyleEngine::parseStyleRule(std::shared_ptr<ASTNode> ruleNode, StyleRuleBucket& bucket) {
    if (!ruleNode) return;
    
    // 获取选择器
    std::string selector;
    if (auto selectorAttr = ruleNode->getAttribute("selector")) {
        if (selectorAttr->isString()) {
            selector = selectorAttr->as<std::string>();
        }
    } else {
        selector = ruleNode->type;
    }
    
    if (selector.empty()) return;
    
    // 支持逗号分隔的多个选择器（例如 ".a .b, .c .d:hover"）
    std::vector<std::string> groups;
    {
        std::stringstream gss(selector);
        std::string tok;
        while (std::getline(gss, tok, ',')) {
            // trim
            size_t a = 0, b = tok.size();
            while (a < b && isspace((unsigned char)tok[a])) ++a;
            while (b > a && isspace((unsigned char)tok[b-1])) --b;
            if (b > a) groups.push_back(tok.substr(a, b-a));
        }
    }

    // 直接存储属性（共享给每个分组）
    std::map<std::string, Attribute> props;
    for (const auto& attrPair : ruleNode->attrs) {
        const std::string& key = attrPair.first;
        const Attribute& attr = attrPair.second;
        if (key == "selector") continue;
        props[key] = attr;
    }

    for (const auto& grp : groups) {
        // 解析伪类，统一通过辅助函数处理
        StyleRule rule;
        std::string cleaned;
        extractPseudoState(grp, cleaned, rule.requiredState);

        rule.selector = cleaned;
        rule.properties = props;

        // 预编译选择器
        if (!rule.selector.empty()) {
            if (rule.selector[0] == '.') {
                rule.compiled.type = CompiledSelector::Type::Class;
                rule.compiled.value = rule.selector.substr(1);
            } else if (rule.selector[0] == '#') {
                rule.compiled.type = CompiledSelector::Type::Id;
                rule.compiled.value = rule.selector.substr(1);
            } else if (rule.selector == "*") {
                rule.compiled.type = CompiledSelector::Type::Universal;
            } else {
                rule.compiled.type = CompiledSelector::Type::Tag;
                rule.compiled.value = rule.selector;
            }
        }

        // 预计算影响标志
        rule.affectsLayout = false;
        rule.affectsPaint = true; // 默认影响绘制
        for (const auto& kv : rule.properties) {
            if (isLayoutProperty(kv.first)) {
                rule.affectsLayout = true;
            }
        }

        bucket.rules.push_back(rule);
    }
}

void StyleEngine::loadDefaultStyles() {
    defaultBucket_.clear();

    // 默认动态样式：按钮 hover/active/focus
    addDefaultStyleRule("button:hover", {
        {"background-color", Attribute("#0069D9")}
        });
    addDefaultStyleRule("button:active", {
        {"background-color", Attribute("#0056ff")},
        {"border-color", Attribute("#003f7f")}
        });

    // 输入框 focus/hover
    //addDefaultStyleRule("input:hover", {
    //    {"background-color", Attribute("#f8f9fa")}
    //    });
    addDefaultStyleRule("input:focus", {
        {"border-color", Attribute("#80bdff")},
        {"box-shadow", Attribute("0 0 0 3px rgba(0,123,255,0.15)")}
        });
    addDefaultStyleRule("button:disabled", {
        {"background-color", Attribute("#555555")},
        {"color", Attribute("#aaaaaa")},
        {"border-color", Attribute("#355653")},
        {"border-width", Attribute(1)},
        {"border-radius", Attribute(6)},
        {"font-size", Attribute(14)},
        {"padding", Attribute("8 16")},
        //{"width", Attribute(100)},
        //{"height", Attribute(25)}
        });
    // Button 默认样式
    addDefaultStyleRule("button", {
        {"background-color", Attribute("#007BFF")},
        {"color", Attribute("#FFFFFF")},
        {"border-color", Attribute("#0056B3")},
        {"border-width", Attribute(1)},
        {"border-radius", Attribute(6)},
        {"font-size", Attribute(14)},
        {"padding", Attribute("8 16")},
        //{"width", Attribute(100)},
        //{"height", Attribute(25)}
    });
    
    // Input 默认样式
    addDefaultStyleRule("input", {
        {"background-color", Attribute("#FFFFFF")},
        {"color", Attribute("#000000")},
        {"border-color", Attribute("#AAAAAA")},
        {"border-width", Attribute(1)},
        {"border-radius", Attribute(4)},
        {"font-size", Attribute(14)},
        {"padding", Attribute("2 4")},
        {"width", Attribute(200)},
        //{"height", Attribute(35)}
    });
    
    // Text 默认样式
    addDefaultStyleRule("text", {
        {"color", Attribute("#000000")},
        {"font-size", Attribute(14)},
        {"width", Attribute("auto")},
        {"height", Attribute("auto")}
    });

    rebuildBucketIndex(defaultBucket_);

}

void StyleEngine::addDefaultStyleRule(const std::string& selector, 
                                     const std::map<std::string, Attribute>& properties) {
    StyleRule rule;
    // Support pseudo-suffix in selector (e.g. "button:hover") for default rules
    std::string cleaned;
    extractPseudoState(selector, cleaned, rule.requiredState);

    rule.selector = cleaned;
    rule.properties = properties;

    // 预编译选择器
    if (!rule.selector.empty()) {
        if (rule.selector[0] == '.') {
            rule.compiled.type = CompiledSelector::Type::Class;
            rule.compiled.value = rule.selector.substr(1);
        } else if (rule.selector[0] == '#') {
            rule.compiled.type = CompiledSelector::Type::Id;
            rule.compiled.value = rule.selector.substr(1);
        } else if (rule.selector == "*") {
            rule.compiled.type = CompiledSelector::Type::Universal;
        } else {
            rule.compiled.type = CompiledSelector::Type::Tag;
            rule.compiled.value = rule.selector;
        }
    }

    // 预计算影响标志
    rule.affectsLayout = false;
    rule.affectsPaint = true;
    for (const auto& kv : rule.properties) {
        const std::string& k = kv.first;
        if (k == "width" || k == "height" || k == "margin" || k == "padding" || 
            k == "display" || k == "position" || k == "flex-direction" || 
            k.find("margin-") == 0 || k.find("padding-") == 0) {
            rule.affectsLayout = true;
        }
    }
    
    // 默认样式写入默认桶
    defaultBucket_.rules.push_back(rule);
}

void StyleEngine::rebuildBucketIndex(StyleRuleBucket& bucket) {
    bucket.index.normal.clear();
    bucket.index.hover.clear();
    bucket.index.active.clear();
    bucket.index.focus.clear();
    bucket.index.dragging.clear();
    bucket.index.disabled.clear();

    for (const auto& rule : bucket.rules) {
        if (rule.requiredState == UIState::None) {
            bucket.index.normal.push_back(&rule);
        } else if (rule.requiredState == UIState::Hover) {
            bucket.index.hover.push_back(&rule);
        } else if (rule.requiredState == UIState::Active) {
            bucket.index.active.push_back(&rule);
        } else if (rule.requiredState == UIState::Focus) {
            bucket.index.focus.push_back(&rule);
        } else if (rule.requiredState == UIState::Dragging) {
            bucket.index.dragging.push_back(&rule);
        } else if (rule.requiredState == UIState::Disabled) {
            bucket.index.disabled.push_back(&rule);
        }
    }
}

std::string StyleEngine::getStyleOwnerId(std::shared_ptr<ASTNode> node) const {
    if (!node) return std::string();

    static const char* kOwnerKeys[] = {
        "__style_owner",
        "style-owner",
        "style_owner"
    };

    for (const auto* key : kOwnerKeys) {
        if (auto attr = node->getAttribute(key)) {
            try {
                if (attr->isString()) {
                    auto owner = attr->as<std::string>();
                    if (!owner.empty()) return owner;
                }
            } catch (...) {
                LDT_ERROR("StyleEngine::getOwner: failed to read attribute");
            }
        }
    }
    return std::string();
}

// Helper to map string attribute keys to StyleProp for cascading weight tracking
static std::optional<ldt::StyleProp> attributeToStyleProp(std::string_view key) {
    if (key == "background-color") return ldt::StyleProp::BackgroundColor;
    if (key == "color") return ldt::StyleProp::TextColor;
    if (key == "border-color") return ldt::StyleProp::BorderColor;
    if (key == "border-width" || key == "border") return ldt::StyleProp::BorderWidth;
    if (key == "border-radius") return ldt::StyleProp::BorderRadius;
    if (key == "font-size") return ldt::StyleProp::FontSize;
    if (key == "line-height") return ldt::StyleProp::LineHeight;
    if (key == "font-family") return ldt::StyleProp::FontFamily;
    if (key == "font-weight") return ldt::StyleProp::FontWeight;
    if (key == "text-align") return ldt::StyleProp::TextAlign;
    if (key == "width") return ldt::StyleProp::Width;
    if (key == "height") return ldt::StyleProp::Height;
    if (key == "overflow") return ldt::StyleProp::Overflow;
    if (key == "box-sizing") return ldt::StyleProp::BoxSizing;
    if (key.rfind("padding", 0) == 0) return ldt::StyleProp::Padding;
    if (key.rfind("margin", 0) == 0) return ldt::StyleProp::Margin;
    if (key.rfind("border-width", 0) == 0 || key == "border") return ldt::StyleProp::BorderWidth;
    if (key == "border-color") return ldt::StyleProp::BorderColor;
    if (key == "border-radius") return ldt::StyleProp::BorderRadius;
    return std::nullopt;
}

ldt::CompiledStyleRules StyleEngine::compile(std::shared_ptr<ASTNode> ast) const {
    ldt::CompiledStyleRules out;
    if (!ast) return out;
    
    // update property weight in the cascader
    auto updatePropWeight = [&](const std::string& key, int order) {
        auto prop = attributeToStyleProp(key);
        if (prop) {
            out.cascader.updateBaseOrder(*prop, order);
            // Handle shorthand side-effects
            if (key == "border") out.cascader.updateBaseOrder(ldt::StyleProp::BorderColor, order);
            if (key == "margin" || key.find("margin-") == 0) out.cascader.updateBaseOrder(ldt::StyleProp::MarginAuto, order);
        }
    };

    auto applyRule = [&](const StyleRule& rule, int currentOrder) {
        StyleCascader::Entry entry;
        entry.requiredState = rule.requiredState;
        entry.affectsLayout = rule.affectsLayout;
        entry.affectsPaint = rule.affectsPaint;
        entry.order = currentOrder;
        applyPropertiesToStyleDelta(entry.delta, rule.properties);

        if (rule.requiredState != ldt::UIState::None) {
            out.cascader.orderedRules.push_back(std::move(entry));
        }

        auto applyToState = [&](CompiledStyleRules::StateIndex idx) {
            auto& state = out.stateRules[idx];
            applyPropertiesToStyleDelta(state.delta, rule.properties);
            state.hasEffect = true;
            if (rule.affectsLayout) {
                state.affectsLayout = true;
                out.hasStatefulLayout = true;
            }
            if (rule.affectsPaint) {
                state.affectsPaint = true;
                out.hasStatefulPaint = true;
            }
        };

        switch (rule.requiredState) {
            case ldt::UIState::None:
                applyPropertiesToComputedStyle(out.base, rule.properties);
                for (const auto& kv : rule.properties) {
                    updatePropWeight(kv.first, currentOrder);
                }
                break;
            case ldt::UIState::Hover:    applyToState(CompiledStyleRules::Hover); break;
            case ldt::UIState::Active:   applyToState(CompiledStyleRules::Active); break;
            case ldt::UIState::Focus:    applyToState(CompiledStyleRules::Focus); break;
            case ldt::UIState::Dragging: applyToState(CompiledStyleRules::Dragging); break;
            case ldt::UIState::Disabled: applyToState(CompiledStyleRules::Disabled); break;
        }
    };

    auto compileBucket = [&](const StyleRuleBucket& bucket, int baseOrder) {
        for (size_t i = 0; i < bucket.rules.size(); ++i) {
            const auto& rule = bucket.rules[i];
            if (!matchesSelector(ast, rule.selector)) continue;
            applyRule(rule, baseOrder + static_cast<int>(i));
        }
    };

    // 默认规则最低优先级
    compileBucket(defaultBucket_, 0);
    // 全局用户规则
    compileBucket(globalBucket_, ORDER_USER_START);

    // 本地规则最高于全局规则
    const std::optional<int> ownerId = ast->getLocalScopeId();
    if (ownerId.has_value()) {
        auto it = localBuckets_.find(ownerId.value());
        if (it != localBuckets_.end()) {
            compileBucket(it->second, ORDER_USER_START + static_cast<int>(globalBucket_.rules.size()));
        }
    }

    // 后处理：处理内联属性（由于内联属性优先级最高，我们在此时就将它们覆盖设置给 base 样式）
    for (const auto& attrPair : ast->attrs) {
        const std::string& key = attrPair.first;
        if (auto prop = attributeToStyleProp(key)) {
            // 通过极大值标记这些属性已被内联样式设置，阻止它们被父节点继承
            std::map<std::string, Attribute> props = {{key, attrPair.second}};
            applyPropertiesToComputedStyle(out.base, props);
            updatePropWeight(key, ORDER_INLINE);
        }
    }

    return out;
}

const Attribute* StyleEngine::getStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property) const {
    return getStyleProperty(node, property, UIState::None);
}

const Attribute* StyleEngine::getStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property, UIState state) const {
    if (!node) return nullptr;

    // 先检查节点自身是否有该属性（内联样式优先级最高）
    if (auto attr = node->getAttribute(property)) {
        return attr;
    }

    auto selectStateList = [&](const StyleRuleBucket& bucket) -> const std::vector<const StyleRule*>* {
        if (state == UIState::None) return nullptr;
        if (hasUIState(state, UIState::Disabled)) return &bucket.index.disabled;
        if (hasUIState(state, UIState::Dragging)) return &bucket.index.dragging;
        if (hasUIState(state, UIState::Focus)) return &bucket.index.focus;
        if (hasUIState(state, UIState::Active)) return &bucket.index.active;
        if (hasUIState(state, UIState::Hover)) return &bucket.index.hover;
        return nullptr;
    };

    // 辅助：在规则索引中查找
    auto findInList = [&](const std::vector<const StyleRule*>& list) -> const Attribute* {
        // 反向遍历以支持覆盖（后面的规则优先级高）
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            const StyleRule* rule = *it;
            // 使用预编译选择器进行快速匹配
            bool match = false;
            switch (rule->compiled.type) {
                case CompiledSelector::Type::Universal:
                    match = true;
                    break;
                case CompiledSelector::Type::Tag:
                    match = (node->type == rule->compiled.value);
                    break;
                case CompiledSelector::Type::Class:
                    match = node->hasClass(rule->compiled.value);
                    break;
                case CompiledSelector::Type::Id:
                    if (auto idAttr = node->getAttribute("id")) {
                        try {
                            if (idAttr->isString() && idAttr->as<std::string>() == rule->compiled.value) {
                                match = true;
                            }
                        } catch (...) {
                            LDT_ERROR("StyleEngine::compile: ID selector match failed");
                        }
                    }
                    break;
            }

            if (match) {
                auto propIt = rule->properties.find(property);
                if (propIt != rule->properties.end()) {
                    return &(propIt->second);
                }
            }
        }
        return nullptr;
    };

    const Attribute* result = nullptr;

    // 读取优先级：local > global > default
    const std::optional<int> ownerId = node->getLocalScopeId();
    if (ownerId.has_value()) {
        auto it = localBuckets_.find(ownerId.value());
        if (it != localBuckets_.end()) {
            if (auto list = selectStateList(it->second)) {
                result = findInList(*list);
                if (result) return result;
            }
            result = findInList(it->second.index.normal);
            if (result) return result;
        }
    }

    if (auto list = selectStateList(globalBucket_)) {
        result = findInList(*list);
        if (result) return result;
    }

    result = findInList(globalBucket_.index.normal);
    if (result) return result;

    if (auto list = selectStateList(defaultBucket_)) {
        result = findInList(*list);
        if (result) return result;
    }

    result = findInList(defaultBucket_.index.normal);

    if (result) return result;

    // --- 继承逻辑 如果没找到，且属性是可继承的，则查找父节点 ---
    if (isInheritableProperty(property)) {
        if (auto parent = node->parent.lock()) {
            return getStyleProperty(parent, property, state);
        }
    }

    return result;
}

std::string StyleEngine::getStylePropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                               const std::string& defaultValue) const {
    auto attr = getStyleProperty(node, property);
    if (!attr) return defaultValue;
    
    try {
        if (attr->isString()) {
            return attr->as<std::string>();
        } else if (attr->isInt()) {
            return std::to_string(attr->as<int>());
        } else if (attr->isFloat()) {
            return std::to_string(attr->as<float>());
        }
    } catch (...) {
        LDT_ERROR("StyleEngine::getStylePropertyString failed to convert value for property=" << property);
    }
    
    return defaultValue;
}

std::string StyleEngine::getStylePropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                               const std::string& defaultValue, UIState state) const {
    auto attr = getStyleProperty(node, property, state);
    if (!attr) return defaultValue;

    try {
        if (attr->isString()) {
            return attr->as<std::string>();
        } else if (attr->isInt()) {
            return std::to_string(attr->as<int>());
        } else if (attr->isFloat()) {
            return std::to_string(attr->as<float>());
        }
    } catch (...) {
        LDT_ERROR("StyleEngine::getStylePropertyString(with state) failed for property=" << property);
    }

    return defaultValue;
}

bool StyleEngine::hasStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property) const {
    return getStyleProperty(node, property) != nullptr;
}

static bool matchesSimpleSelector(std::shared_ptr<ASTNode> node, const std::string& selector) {
    if (!node || selector.empty()) return false;
    if (selector == "*") return true;

    if (selector[0] == '#') {
        // ID Selector
        std::string id = selector.substr(1);
        if (auto idAttr = node->getAttribute("id")) {
            try {
                return idAttr->isString() && idAttr->as<std::string>() == id;
            } catch (...) {
                LDT_ERROR("StyleEngine::matchesSimpleSelector: failed to compare id");
            }
        }
        return false;
    } else if (selector[0] == '.') {
        // Class Selector
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
        // Type Selector
        return node->type == selector;
    }
}

bool StyleEngine::matchesSelector(std::shared_ptr<ASTNode> node, const std::string& selector) const {
    if (!node || selector.empty()) return false;
    
    // Split selector by space
    std::vector<std::string> parts;
    std::stringstream ss(selector);
    std::string item;
    while (std::getline(ss, item, ' ')) {
        if (!item.empty()) parts.push_back(item);
    }

    if (parts.empty()) return false;

    // Match the last part against the current node
    if (!matchesSimpleSelector(node, parts.back())) return false;

    // If there are more parts, walk up the tree
    if (parts.size() > 1) {
        auto current = node->parent.lock();
        int partIdx = (int)parts.size() - 2;
        
        while (current && partIdx >= 0) {
            if (matchesSimpleSelector(current, parts[partIdx])) {
                partIdx--;
            }
            current = current->parent.lock();
        }
        
        // If we matched all parts, return true
        return partIdx < 0;
    }

    return true;
}

void StyleEngine::clearStyles() {
    defaultBucket_.clear();
    globalBucket_.clear();
    localBuckets_.clear();
}
