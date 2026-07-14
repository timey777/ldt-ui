#pragma once

#include "container_control.h"

namespace ldt {

/**
 * @brief 面板控件
 * 
 * 一个容器控件，可以包含其他控件，并绘制背景和边框
 */
class LDT_API Panel : public ContainerControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Panel() = default;
    friend class ControlFactory;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override {
        // 绘制面板背景
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

        // 渲染所有子控件，clip 沿递归链路下传
        renderChildren(displayList, clip);
    }

public:
    std::string getTypeName() const override { return "Panel"; }

    // 内边距
    void setPadding(float padding) {
        paddingTop_ = paddingRight_ = paddingBottom_ = paddingLeft_ = padding;
    }
    
    void setPadding(float top, float right, float bottom, float left) {
        paddingTop_ = top;
        paddingRight_ = right;
        paddingBottom_ = bottom;
        paddingLeft_ = left;
    }

    float getPaddingTop() const { return paddingTop_; }
    float getPaddingRight() const { return paddingRight_; }
    float getPaddingBottom() const { return paddingBottom_; }
    float getPaddingLeft() const { return paddingLeft_; }

private:
    float paddingTop_ = 0.0f;
    float paddingRight_ = 0.0f;
    float paddingBottom_ = 0.0f;
    float paddingLeft_ = 0.0f;
};

} // namespace ldt

