#pragma once
#include <string>
#include <array>
#include "render/display_list.h"
#include "node_flags.h"
#include "computed_style.h" // For LayoutUnit, ui::Edges etc.

namespace ldt {

    /**
     * @brief 属性提供者模型
     * 盒模型引擎(BoxModelEngine)通过此接口获取所需数据，
     * 而不需要关心属性当前是定义在 @style 还是 @layout 块中。
     */
    class IPropertyProvider {
    public:
        virtual ~IPropertyProvider() = default;

        // --- 盒模型核心几何属性 ---
        virtual LayoutUnit getWidth() const = 0;
        virtual LayoutUnit getHeight() const = 0;
        virtual ui::Edges getPadding() const = 0;
        virtual ui::Edges getMargin() const = 0;
        virtual std::array<bool, 4> getMarginAuto() const = 0;
        virtual ui::Edges getBorderWidth() const = 0;

        // --- 布局上下文属性 ---
        virtual FormattingContext getDisplay() const = 0;
        virtual Position getPosition() const = 0;

        // --- 文字与内容测量属性 ---
        virtual float getFontSize() const = 0;
        virtual float getLineHeight() const = 0;
        virtual std::string getFontFamily() const = 0;
        virtual FontWeight getFontWeight() const = 0;
        virtual TextAlign getTextAlign() const = 0;
        virtual Overflow getOverflow() const = 0;
        virtual BoxSizing getBoxSizing() const = 0;

        // --- 悬浮/绝对定位属性 ---
        virtual bool isOverlay() const = 0;
        virtual Anchor getAnchor() const = 0;
        virtual float getOffsetX() const = 0;
        virtual float getOffsetY() const = 0;

        // --- Flex 布局属性 ---
        virtual FlexDirection getFlexDirection() const = 0;
        virtual AlignItems getAlignItems() const = 0;
        virtual JustifyContent getJustifyContent() const = 0;
        virtual FlexWrap getFlexWrap() const = 0;
        virtual float getFlexGrow() const = 0;
        virtual float getFlexShrink() const = 0;
        virtual float getGap() const = 0;
    };


} // namespace ldt
