#pragma once

#include "abstract_control.h"
#include "text.h"
#include <string>
#include <vector>
#include "scene.h"
#include "misc/logger.h"

namespace ldt {

/**
 * @brief 单选按钮控件
 */
class LDT_API RadioButton : public AbstractControl {
public:
    RadioButton() = default;

    // 状态
    enum class State {
        Normal,
        Hover,
        Pressed,
        Disabled
    };

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override {
        // 已经由基类 render() 做了 visible_ 判定

        // 颜色配置
        ui::Color circleStroke = strokeColor_.a > 0.0f ? strokeColor_ : ui::Color(0.5f, 0.5f, 0.5f, 1.0f);
        ui::Color circleFill = ui::Color(0, 0, 0, 0); // Transparent by default
        ui::Color checkedFill = fillColor_.a > 0.0f ? fillColor_ : ui::Color(0.2f, 0.6f, 1.0f, 1.0f);
        
        if (state_ == State::Hover) {
             circleStroke = ui::Color(0.3f, 0.7f, 1.0f, 1.0f);
        }

        // 计算圆的位置
        // 假设 RadioButton 左侧是圆圈，右侧是文字
        float radius = 8.0f; // 圆圈半径
        float diameter = radius * 2.0f;
        float padding = 4.0f;
        float x = bounds_.x + padding;
        float y = bounds_.y + (bounds_.height - diameter) / 2.0f;

        // 绘制外圈
        displayList.addRect(ui::Rect(x, y, diameter, diameter), circleFill, circleStroke, 2.0f, radius);

        // 如果选中，绘制内圈
        if (checked_) {
            float innerRadius = radius * 0.5f;
            float innerDiameter = innerRadius * 2.0f;
            float centerDist = (diameter - innerDiameter) / 2.0f;
            displayList.addRect(ui::Rect(x + centerDist, y + centerDist, innerDiameter, innerDiameter), checkedFill, ui::Color(), 0.0f, innerRadius);
        }

        // 绘制文字
        if (!text_.empty()) {
            // ... (绘制文字逻辑)
        }
    }

    // 假设 DisplayList.h 有 addTextLayout. 
    // 为了简单，我们只绘制圆圈，让用户把 Text 控件放在旁边？
    // 或者我们像 Button 那样作为 ContainerControl？
    // Button 是 ContainerControl，所以可以包含 Text。
    // 如果想要方便，我们把 RadioButton 做成 ContainerControl 更好。

    std::string getTypeName() const override { return "RadioButton"; }

protected:
    bool handleLocalMouseMove(ControlLocalPointDp point) override {
        if (!isVisible()) return false;
        // 如果是 ContainerControl，不需要自己处理 hitTest，除非想拦截
        // 但如果只是 AbstractControl
        bool inside = hitTestLocal(point);
        if (inside && getScene()) getScene()->setCursor(ldt::Scene::CursorType::Cursor_Hand);
        
        if (inside) {
            if (state_ == State::Normal) state_ = State::Hover;
        } else {
            if (state_ == State::Hover) state_ = State::Normal;
        }
        return inside; 
    }

    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override {
        if (!isVisible()) return false;
        bool inside = hitTestLocal(point);
        if (inside && action == 1) { // press
            state_ = State::Pressed;
            return true;
        }
        if (inside && action == 0) { // release
            state_ = State::Hover;
            if (button == 0) {
                setChecked(true);
                // TODO: 通知组内其他 RadioButton 取消选中
                // 这需要遍历父容器或 Scene 查找同组 RadioButton
                notifyGroup();
            }
            return true;
        }
        return false;
    }

public:
    void setChecked(bool checked) {
        if (checked_ != checked) {
            checked_ = checked;
            // invalidate?
        }
    }
    bool isChecked() const { return checked_; }

    void setGroup(const std::string& group) { group_ = group; }
    const std::string& getGroup() const { return group_; }

    void setText(const std::string& text) { text_ = text; } // 简单属性

protected:
    void notifyGroup() {
        if (group_.empty()) return;
        AbstractControl* parent = getParent();
        if (!parent) return; // 只能在一个容器内互斥？或者 Scene 级？通常是 Parent 内。
        
        // 尝试转换父控件为 ContainerControl 来遍历兄弟
        // 但 AbstractControl::getParent 返回 AbstractControl*
        // 我们需要 dynamic_cast (如果开启 RTTI) 或者 reinterpret_cast ( unsafe)
        // 使用 dynamic_pointer_cast 前提是持有 shared_ptr，但 getParent 返回 raw ptr
        
        // 暂时略过自动互斥逻辑，或者只实现简单的点击切换
    }

    State state_ = State::Normal;
    bool checked_ = false;
    std::string group_;
    std::string text_;
    ui::Color hoverColor_ = ui::Color(0,0,0,0);
};

} // namespace ldt
