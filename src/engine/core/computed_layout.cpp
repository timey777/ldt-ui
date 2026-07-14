#include "computed_layout.h"

using namespace ldt;

const ldt::ui::Rect& ComputedLayout::getContentBox() const { return contentBox_; }
const ldt::ui::Rect& ComputedLayout::getPaddingBox() const { return paddingBox_; }
const ldt::ui::Rect& ComputedLayout::getBorderBox() const { return borderBox_; }
const ldt::ui::Rect& ComputedLayout::getMarginBox() const { return marginBox_; }

ldt::ui::Rect ComputedLayout::getAbsolutePaddingBox() const {
    return {
        borderBox_.x + paddingBox_.x,
        borderBox_.y + paddingBox_.y,
        paddingBox_.width,
        paddingBox_.height
    };
}

ldt::ui::Rect ComputedLayout::getAbsoluteContentBox() const {
    return {
        borderBox_.x + contentBox_.x,
        borderBox_.y + contentBox_.y,
        contentBox_.width,
        contentBox_.height
    };
}

void ComputedLayout::finalizeSizes() {
    float mLeft = marginLeftAuto ? 0.0f : margin.left;
    float mRight = marginRightAuto ? 0.0f : margin.right;
    float mTop = marginTopAuto ? 0.0f : margin.top;
    float mBottom = marginBottomAuto ? 0.0f : margin.bottom;

    contentBox_.width = computedWidth;
    contentBox_.height = computedHeight;

    paddingBox_.width = computedWidth + padding.horizontal();
    paddingBox_.height = computedHeight + padding.vertical();

    borderBox_.width = paddingBox_.width + border.horizontal();
    borderBox_.height = paddingBox_.height + border.vertical();

    marginBox_.width = borderBox_.width + mLeft + mRight;
    marginBox_.height = borderBox_.height + mTop + mBottom;

    paddingBox_.x = border.left;
    paddingBox_.y = border.top;

    contentBox_.x = border.left + padding.left;
    contentBox_.y = border.top + padding.top;
}

void ComputedLayout::setPosition(float absX, float absY) {
    marginBox_.x = absX;
    marginBox_.y = absY;

    float resolvedMarginLeft = marginLeftAuto ? 0.0f : margin.left;
    float resolvedMarginTop = marginTopAuto ? 0.0f : margin.top;

    borderBox_.x = marginBox_.x + resolvedMarginLeft;
    borderBox_.y = marginBox_.y + resolvedMarginTop;
}