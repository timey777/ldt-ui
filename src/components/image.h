#pragma once

#include "abstract_control.h"

namespace ldt {

/**
 * @brief 图片控件
 * 
 * 用于显示图片内容
 */
class LDT_API Image : public AbstractControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Image() = default;
    explicit Image(const std::string& src) : src_(src) {}
    friend class ControlFactory;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override {
        if (src_.empty()) return;

        // 只有当有背景色或有边框时才绘制矩形背景
        bool hasFill = (fillColor_.a > 0.0f);
        bool hasStroke = (strokeWidth_.any() && strokeColor_.a > 0.0f);

        if (hasFill || hasStroke) {
            displayList.addRect(
                bounds_,
                fillColor_,
                strokeColor_,
                strokeWidth_,
                cornerRadius_
            );
        }

        if (!backgroundImage_.empty()) {
            displayList.addImage(backgroundImage_, bounds_);
        }

        // 计算内容区域（减去内边距）
        ui::Rect contentBounds = bounds_;
        contentBounds.x += paddingLeft_;
        contentBounds.y += paddingTop_;
        contentBounds.width -= (paddingLeft_ + paddingRight_);
        contentBounds.height -= (paddingTop_ + paddingBottom_);

        if (contentBounds.width > 0 && contentBounds.height > 0) {
            displayList.addImage(src_, contentBounds);
        }
    }

public:
    std::string getTypeName() const override { return "Image"; }

    void setSrc(const std::string& src) { src_ = src; }
    const std::string& getSrc() const { return src_; }

private:
    std::string src_;
};

} // namespace ldt
