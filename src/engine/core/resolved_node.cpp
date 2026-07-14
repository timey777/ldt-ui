#include "resolved_node.h"
#include "engine/document_runtime.h"
#include "engine/style_engine.h"
#include "engine/layout_engine.h"
#include "engine/update_scheduler.h"
#include "misc/stable_id.h"
#include "components/abstract_control.h"
#include <algorithm>
#include <components/control_factory.h>
#include "engine/core/ast_node.h"
#include "engine/core/property_resolver.h"
#include "misc/logger.h"

using namespace ldt;

// ============ ResolvedNode 实现 ============

ldt::ResolvedNode::ResolvedNode(ResolvedTree* tree) : tree_(tree), parent(nullptr) {
    property_resolver_ = new PropertyResolver(this);
}
ldt::ResolvedNode::~ResolvedNode() {
    delete (PropertyResolver*)property_resolver_;
}


const IPropertyProvider& ResolvedNode::props() const {
    return *property_resolver_;
}

// Helper to merge StyleDelta into ComputedStyle
static void mergeStyle(ldt::ComputedStyle& target, const ldt::StyleDelta& delta) {
    if (delta.has(ldt::StyleProp::BackgroundColor)) target.backgroundColor = delta.backgroundColor;

    if (delta.has(ldt::StyleProp::TextColor)) target.textColor = delta.textColor;
    if (delta.has(ldt::StyleProp::BorderColor)) target.borderColor = delta.borderColor;
    if (delta.has(ldt::StyleProp::BorderWidth)) target.borderWidth = delta.borderWidth;
    if (delta.has(ldt::StyleProp::BorderRadius)) target.borderRadius = delta.borderRadius;
    if (delta.has(ldt::StyleProp::FontSize)) target.fontSize = delta.fontSize;
    if (delta.has(ldt::StyleProp::LineHeight)) target.lineHeight = delta.lineHeight;
    if (delta.has(ldt::StyleProp::FontFamily)) target.fontFamily = delta.fontFamily;
    if (delta.has(ldt::StyleProp::FontWeight)) target.fontWeight = delta.fontWeight;
    if (delta.has(ldt::StyleProp::TextAlign)) target.textAlign = delta.textAlign;
    if (delta.has(ldt::StyleProp::Width)) target.width = delta.width;
    if (delta.has(ldt::StyleProp::Height)) target.height = delta.height;
    if (delta.has(ldt::StyleProp::Overflow)) target.overflow = delta.overflow;
    if (delta.has(ldt::StyleProp::BoxSizing)) target.boxSizing = delta.boxSizing;
    if (delta.has(ldt::StyleProp::Padding)) {
        target.padding = delta.padding;
    }
    if (delta.has(ldt::StyleProp::Margin)) {
        target.margin = delta.margin;
    }
    if (delta.has(ldt::StyleProp::MarginAuto)) {
        for(int i=0; i<4; ++i) target.marginAuto[i] = delta.marginAuto[i];
    }
    if (delta.has(ldt::StyleProp::Opacity)) target.opacity = delta.opacity;
    if (delta.has(ldt::StyleProp::Visible)) target.visible = delta.visible;
    if (delta.has(ldt::StyleProp::BoxShadow)) target.boxShadow = delta.boxShadow;
    if (delta.has(ldt::StyleProp::BackgroundImage)) target.backgroundImage = delta.backgroundImage;
}

static void mergeStyleWithOrder(ldt::ComputedStyle& target,
    const ldt::StyleDeclaration& delta,
    const ldt::StyleCascader& cascader,
    int order) {
    
    struct ApplyEntry {
        StyleProp prop;
        void (*apply)(ldt::ComputedStyle&, const ldt::StyleDeclaration&);
    };

    // Table of property application logic
    static const ApplyEntry entries[] = {
        {StyleProp::BackgroundColor, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.backgroundColor = d.backgroundColor; }},
        {StyleProp::TextColor, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.textColor = d.textColor; }},
        {StyleProp::BorderColor, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.borderColor = d.borderColor; }},
        {StyleProp::BorderWidth, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.borderWidth = d.borderWidth; }},
        {StyleProp::BorderRadius, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.borderRadius = d.borderRadius; }},
        {StyleProp::FontSize, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.fontSize = d.fontSize; }},
        {StyleProp::LineHeight, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.lineHeight = d.lineHeight; }},
        {StyleProp::FontFamily, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.fontFamily = d.fontFamily; }},
        {StyleProp::FontWeight, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.fontWeight = d.fontWeight; }},
        {StyleProp::TextAlign, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.textAlign = d.textAlign; }},
        {StyleProp::Width, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.width = d.width; }},
        {StyleProp::Height, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.height = d.height; }},
        {StyleProp::Overflow, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.overflow = d.overflow; }},
        {StyleProp::BoxSizing, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.boxSizing = d.boxSizing; }},
        {StyleProp::Padding, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) {
                t.padding = d.padding;
            }},
        {StyleProp::Margin, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) {
                t.margin = d.margin;
            }},
        {StyleProp::MarginAuto, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) {
                for (int i = 0; i < 4; ++i) t.marginAuto[i] = d.marginAuto[i];
            }},
        {StyleProp::Opacity, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.opacity = d.opacity; }},
        {StyleProp::Visible, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.visible = d.visible; }},
        {StyleProp::BoxShadow, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.boxShadow = d.boxShadow; }},
        {StyleProp::BackgroundImage, [](ldt::ComputedStyle& t, const ldt::StyleDeclaration& d) { t.backgroundImage = d.backgroundImage; }},
    };

    for (const auto& entry : entries) {
        // Only apply if the delta has the value AND it can override the base style order
        if (delta.has(entry.prop) && cascader.canOverride(entry.prop, order)) {
            entry.apply(target, delta);
        }
    }
}

// ResolvedNode simple accessors and state helpers (moved from header)
const CompiledStyleRules& ResolvedNode::getCompiledStyleRules() const { return compiledRules; }
void ResolvedNode::setCompiledStyleRules(const CompiledStyleRules& c) { compiledRules = c; recomputeStyle(); }
void ResolvedNode::setCompiledLayoutRules(const CompiledLayoutRules& c) { layoutRules = c; }

std::weak_ptr<ldt::AbstractControl> ResolvedNode::getControl() { return this->control; }
void ResolvedNode::setControl(std::weak_ptr<ldt::AbstractControl> control) { this->control = control; }

void ResolvedNode::setUIState(UIState newState) {
    if (uiState == newState) return;
    uiState = newState;
    recomputeStyle();
}

bool ResolvedNode::hasUIState(UIState s) const {
    return ldt::hasUIState(uiState, s);
}

void ResolvedNode::addUIState(UIState s) {
    if (this->hasUIState(s)) return;
    uiState = uiState | s;
    recomputeStyle();
}

void ResolvedNode::removeUIState(UIState s) {
    if (!this->hasUIState(s)) return;
    uiState = static_cast<UIState>(static_cast<uint8_t>(uiState) & ~static_cast<uint8_t>(s));
    recomputeStyle();
}

void ResolvedNode::clearDirty(DirtyFlag flags) {
    dirtyFlags = static_cast<DirtyFlag>(static_cast<uint32_t>(dirtyFlags) & ~static_cast<uint32_t>(flags));
}

bool ResolvedNode::isDirty(DirtyFlag flags) const { return hasDirtyFlag(dirtyFlags, flags); }

bool ResolvedNode::needsLayout() const { return isDirty(DirtyFlag::Layout); }
bool ResolvedNode::needsStyle() const { return isDirty(DirtyFlag::Style); }
DirtyFlag ResolvedNode::getDirtyFlags() const { return dirtyFlags; }

std::string ResolvedNode::getId()
{
    if (astNode->hasAttribute("id")) return astNode->getAttribute("id")->as<std::string>();
    return std::string();
}

std::string ResolvedNode::getUid()
{
    if (astNode->getUid()) return astNode->getUid()->as<std::string>();
    return std::string();
}

const std::vector<ResolvedNode*>& ldt::ResolvedNode::getChildren()
{
    return children;
}
ldt::ResolvedNode* ldt::ResolvedNode::at(int i)
{
    if (i < children.size())
    {
        return children[i];
    }
    return nullptr;
}
const std::vector<ResolvedNode*>& ldt::ResolvedNode::getFlowChildren()
{
    return flow_children;
}
const std::vector<ResolvedNode*>& ldt::ResolvedNode::getOverlayChildren()
{
    return overlay_children;
}

void ldt::ResolvedNode::clear()
{
    children.clear();
    flow_children.clear();
    overlay_children.clear();
}

void ldt::ResolvedNode::destory()
{
    if (parent) {
        parent->extractChild(this);
    }

    for (auto* child : children) {
        if (child) {
            child->parent = nullptr;
        }
    }

    clear();
    control.reset();
    astNode.reset();
}

void ldt::ResolvedNode::addChildren(ResolvedNode* node)
{
    if (!node) {
        return;
    }
    children.push_back(node);
    if (!node->finalStyle.overlay)
    {
        flow_children.push_back(node);
    }
    else
    {
        overlay_children.push_back(node);
    }
}

ResolvedNode* ldt::ResolvedNode::extractChild(ResolvedNode* node)
{
    if (!node) return nullptr;

    auto it = std::find_if(children.begin(), children.end(),
        [&](ResolvedNode* child) { return child == node; });
    if (it == children.end()) {
        return nullptr;
    }

    auto extracted = *it;
    children.erase(it);

    auto eraseByPtr = [&](std::vector<ResolvedNode*>& bucket) {
        bucket.erase(std::remove_if(bucket.begin(), bucket.end(),
            [&](ResolvedNode* child) { return child == node; }), bucket.end());
    };

    eraseByPtr(flow_children);
    eraseByPtr(overlay_children);

    extracted->parent = nullptr;
    return extracted;
}

void ResolvedNode::applyInheritedStyles() {
    // 如果当前节点没有在基础样式中明确设置某些可继承属性，则从父节点继承
    if (!parent) return;

    auto& cascader = compiledRules.cascader;
    auto& pStyle = parent->finalStyle;

    // 检查属性是否未被当前节点的任何 “用户定义规则” 或 “内联样式” 设置
    // 小于 ORDER_USER_START 的都被视为未设置或默认值，允许从父节点继承
    auto isNotSet = [&](StyleProp p) {
        return cascader.basePropertyOrders[(size_t)p] < StyleEngine::ORDER_USER_START;
    };

    if (isNotSet(StyleProp::TextColor))   finalStyle.textColor = pStyle.textColor;
    if (isNotSet(StyleProp::FontSize))    finalStyle.fontSize = pStyle.fontSize;
    if (isNotSet(StyleProp::FontFamily))  finalStyle.fontFamily = pStyle.fontFamily;
    if (isNotSet(StyleProp::FontWeight))  finalStyle.fontWeight = pStyle.fontWeight;
    if (isNotSet(StyleProp::LineHeight))  finalStyle.lineHeight = pStyle.lineHeight;
    if (isNotSet(StyleProp::TextAlign))   finalStyle.textAlign = pStyle.textAlign;
    if (isNotSet(StyleProp::Visible))     finalStyle.visible = pStyle.visible;
}

void ResolvedNode::recomputeStyle() {
    // 检查优化：如果只是 UIState 变化且没有命中任何规则，则跳过重算
    // 若 uiState == lastAppliedState，说明可能是规则变了（或强制重算），则必须执行
    if (uiState != lastAppliedState) {
        UIState diff = static_cast<UIState>(static_cast<uint8_t>(uiState) ^ static_cast<uint8_t>(lastAppliedState));
        bool relevant = false;

        auto checkState = [&](UIState flag, CompiledStyleRules::StateIndex idx) {
            if (ldt::hasUIState(diff, flag) && compiledRules.stateRules[idx].hasEffect) relevant = true;
        };

        checkState(UIState::Hover, CompiledStyleRules::Hover);
        checkState(UIState::Active, CompiledStyleRules::Active);
        checkState(UIState::Focus, CompiledStyleRules::Focus);
        checkState(UIState::Dragging, CompiledStyleRules::Dragging);
        checkState(UIState::Disabled, CompiledStyleRules::Disabled);

        if (!relevant) {
            lastAppliedState = uiState;
            return;
        }
    }

    finalStyle = compiledRules.base;

    // --- 样式继承逻辑 ---
    applyInheritedStyles();

    // Determine if layout might be affected by current state change
    bool layoutAffected = false;

    // Apply state rules in source order.
    // State rules should override non-inline base declarations, but must not override inline styles.
    constexpr int kStateOverrideOrder = StyleEngine::ORDER_INLINE - 1;
    for (const auto& entry : compiledRules.cascader.orderedRules) {
        if (entry.requiredState == UIState::None || hasUIState(entry.requiredState)) {
            mergeStyleWithOrder(finalStyle, entry.delta, compiledRules.cascader, kStateOverrideOrder);
            if (entry.affectsLayout) layoutAffected = true;
        }
    }

    // 更新最后应用的状态
    lastAppliedState = uiState;

    // 状态变化一定影响样式和重绘
    DirtyFlag flags = DirtyFlag::Style | DirtyFlag::Paint;
    if (layoutAffected) flags = flags | DirtyFlag::Layout;
    
    markDirty(flags);
}

void ldt::ResolvedNode::recompileStyleAndLayout(DocumentRuntime* ctx)
{
    if (ctx) {
        auto se = ctx->getStyleEngine();
        auto le = ctx->getLayoutEngine();
        if (se) {
            try {
                setCompiledStyleRules(se->compile(astNode));
            }
            catch (...) {
                LDT_ERROR("ResolvedNode::recompileStyleAndLayout: StyleEngine::compile threw");
            }
        }
        if (le) {
            try {
                setCompiledLayoutRules(le->compile(astNode));
            }
            catch (...) {
                LDT_ERROR("ResolvedNode::recompileStyleAndLayout: LayoutEngine::compile threw");
            }
        }
    }
}

void ResolvedNode::markDirty(DirtyFlag flags) {
    dirtyFlags |= flags;
    UpdateScheduler::getInstance().requestUpdate(this);
    
    // 如果布局或样式变脏，需要向上传播
    if (hasDirtyFlag(flags, DirtyFlag::Layout) || hasDirtyFlag(flags, DirtyFlag::Style)) {
        propagateDirtyToParent();
    }
    
    // 如果样式变脏，且控件存在，尝试标记控件层脏（尤其是容器控件需要刷新滚动条）
    if (hasDirtyFlag(flags, DirtyFlag::Style)) {
        if (auto ctrl = control.lock()) {
            ctrl->markLayerDirty();
        }
    }

    // 如果当前节点布局变脏，所有子节点也需要重新布局
    if (hasDirtyFlag(flags, DirtyFlag::Layout)) {
        for (auto& child : children) {
            if (child) {
                child->markDirty(DirtyFlag::Layout | DirtyFlag::Paint);
            }
        }
    }
}

void ResolvedNode::markDirtyNoCascade(DirtyFlag flags) {
    dirtyFlags |= flags;
    UpdateScheduler::getInstance().requestUpdate(this);
    
    if (hasDirtyFlag(flags, DirtyFlag::Layout) || hasDirtyFlag(flags, DirtyFlag::Style)) {
        propagateDirtyToParent();
    }
    
    if (hasDirtyFlag(flags, DirtyFlag::Style)) {
        if (auto ctrl = control.lock()) {
            ctrl->markLayerDirty();
        }
    }
    // Intentionally NOT cascading to children — caller handles subtree layout
}

void ResolvedNode::propagateDirtyToParent() {
    if (parent) {
        // 子节点变脏时，父节点需要重新布局子节点
        parent->dirtyFlags |= DirtyFlag::ChildrenLayout;
        parent->propagateDirtyToParent();
    }
}

Anchor ldt::stringToAnchor(const std::string& str)
{
    if (str == "none") return Anchor::None;
    if (str == "topleft") return Anchor::TopLeft;
    if (str == "center") return Anchor::Center;
    if (str == "topright") return Anchor::TopRight;
    if (str == "bottomleft") return Anchor::BottomLeft;
    if (str == "bottomright") return Anchor::BottomRight;

    // 默认值或抛出异常
    return Anchor::None;
    // 或者: throw std::invalid_argument("Invalid Anchor string: " + str);
}
