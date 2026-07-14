#pragma once
#include "property_provider.h"
#include "resolved_node.h"

namespace ldt {

/**
 * @brief 节点属性解析器 (Adapter)
 * 将 ResolvedNode 的内部存储（finalStyle 和 layoutRules）映射到 IPropertyProvider 接口。
 * 这解耦了布局引擎与节点的具体存储结构。
 */
class PropertyResolver : public IPropertyProvider {
public:
    explicit PropertyResolver(const ResolvedNode* n) : node(n) {}
    explicit PropertyResolver(const std::shared_ptr<ResolvedNode>& n) : node(n.get()) {}

    // --- IPropertyProvider 实现 ---
    
    // 几何属性 (目前从 finalStyle 获取)
    LayoutUnit getWidth() const override { return node->finalStyle.width; }
    LayoutUnit getHeight() const override { return node->finalStyle.height; }
    ui::Edges getPadding() const override { return node->finalStyle.padding; }
    ui::Edges getMargin() const override { return node->finalStyle.margin; }
    std::array<bool, 4> getMarginAuto() const override { return node->finalStyle.marginAuto; }
    ui::Edges getBorderWidth() const override { return node->finalStyle.borderWidth; }

    // 布局上下文
    FormattingContext getDisplay() const override { return node->layoutRules.displayEnum; }
    Position getPosition() const override { return node->layoutRules.position; }

    // 文字与内容
    float getFontSize() const override { return node->finalStyle.fontSize; }
    float getLineHeight() const override { return node->finalStyle.lineHeight; }
    std::string getFontFamily() const override { return node->finalStyle.fontFamily; }
    FontWeight getFontWeight() const override { return node->finalStyle.fontWeight; }
    TextAlign getTextAlign() const override { return node->finalStyle.textAlign; }
    Overflow getOverflow() const override { return node->finalStyle.overflow; }
    BoxSizing getBoxSizing() const override { return node->finalStyle.boxSizing; }

    // 悬浮/绝对定位
    bool isOverlay() const override { return node->finalStyle.overlay; }
    Anchor getAnchor() const override { return node->finalStyle.anchor; }
    float getOffsetX() const override { return node->finalStyle.offsetX; }
    float getOffsetY() const override { return node->finalStyle.offsetY; }

    // Flex 布局 (目前大都存储在 layoutRules)
    FlexDirection getFlexDirection() const override { return node->layoutRules.flexDirection; }
    AlignItems getAlignItems() const override { return node->layoutRules.alignItems; }
    JustifyContent getJustifyContent() const override { return node->layoutRules.justifyContent; }
    FlexWrap getFlexWrap() const override { return node->layoutRules.flexWrap; }
    float getFlexGrow() const override { return node->layoutRules.flexGrow; }
    float getFlexShrink() const override { return node->layoutRules.flexShrink; }
    float getGap() const override { return node->layoutRules.gap; }

private:
    const ResolvedNode* node;
};

} // namespace ldt
