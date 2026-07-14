#pragma once

#include "container_control.h"
#include "text.h"
#include <iostream>
#include "scene.h"
#include "misc/logger.h"

namespace ldt {

/**
 * @brief 按钮控件
 * 
 * 可交互的按钮控件，可以选择性地添加文本或其他子控件
 * 设计为容器控件，提供更大的灵活性
 */
class LDT_API Button : public ContainerControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Button() = default;
    friend class ControlFactory;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override {
        // 根据状态选择颜色
        ui::Color bgColor = fillColor_;

        // 绘制按钮背景
        displayList.addRect(
            bounds_,
            bgColor,
            strokeColor_,
            strokeWidth_,
            cornerRadius_
        );

        if (!backgroundImage_.empty()) {
            displayList.addImage(backgroundImage_, bounds_);
        }

        // 渲染所有子控件（可能包含文本、图标等），clip 沿递归链路下传
        renderChildren(displayList, clip);
    }

public:
    std::string getTypeName() const override { return "Button"; }

protected:
    // 事件处理：鼠标悬停/按下改变状态
    bool handleLocalMouseMove(ControlLocalPointDp point) override {
        if (!isVisible()) return false;
        bool inside = hitTestLocal(point);
        if (inside && getScene()) getScene()->setCursor(ldt::Scene::CursorType::Cursor_Hand);
        return inside; // if inside, we consider the event handled
    }

    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override {
        if (!isVisible()) return false;
        if (button != 0) {
            // GLFW_MOUSE_BUTTON_LEFT == 0 in many platforms; but keep generic
        }
        bool inside = hitTestLocal(point);
        if (inside && action == 1) { // press
            return true;
        }
        if (inside && action == 0) { // release
            // TODO: 点击回调（暂时打印）
            try { LDT_LOG("[Button] clicked: " << getId()); } catch(...) {}
            return true;
        }
        return false;
    }

private:
};

} // namespace ldt

