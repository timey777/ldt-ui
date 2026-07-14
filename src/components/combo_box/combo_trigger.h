#pragma once

#include "../container_control.h"

namespace ldt {

class ComboBox;

/**
 * @brief 下拉框触发按钮
 */
class ComboTrigger : public ContainerControl {
public:
    explicit ComboTrigger(ComboBox* owner);

    std::string getTypeName() const override { return "ComboTrigger"; }
    
    void setText(const std::string& text);
    void setTextColor(const ui::Color& color) { textColor_ = color; }
    void setFontSize(float size) { fontSize_ = size; }

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

protected:
    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override;
    bool handleLocalHitTest(ControlLocalPointDp point) const override;

private:
    ComboBox* owner_ = nullptr;
    std::string text_;
    ui::Color textColor_ = ui::Color(0, 0, 0, 1);
    float fontSize_ = 14.0f;
};

} // namespace ldt
