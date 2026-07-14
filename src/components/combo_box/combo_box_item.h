#pragma once

#include "../abstract_control.h"
#include <functional>

namespace ldt {

/**
 * @brief 下拉框项控件
 * 
 * 用于显示下拉框中的单个选项
 */
class ComboBoxItem : public AbstractControl {
public:
    ComboBoxItem(const std::string& text, int index, std::function<void(int)> onClick);

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;
    
    std::string getTypeName() const override { return "ComboBoxItem"; }

protected:
    bool handleLocalMouseMove(ControlLocalPointDp point) override;
    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override;

public:
    std::string getText() const { return text_; }
    
    void setSelected(bool selected) { isSelected_ = selected; }
    bool isSelected() const { return isSelected_; }

private:
    std::string text_;
    int index_;
    std::function<void(int)> onClick_;
    bool isHovered_ = false;
    bool isSelected_ = false;
};

} // namespace ldt
