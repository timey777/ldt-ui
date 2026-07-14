#pragma once

#include "../container_control.h"
#include "combo_box_item.h"
#include "combo_box_popup.h"
#include "combo_trigger.h"

namespace ldt {

/**
 * @brief 下拉框控件
 * 
 * 提供选项下拉功能的控件
 */
class LDT_API ComboBox : public ContainerControl {
public:
    ComboBox();

    std::string getTypeName() const override { return "ComboBox"; }

    void setOptions(const std::vector<std::string>& options);
    
    void setSelectedIndex(int idx);
    
    int getSelectedIndex() const { return selectedIndex_; }

    void setTextColor(const ui::Color& color);
    void setFontSize(float size);

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

    void setBounds(const ui::Rect& b) override;

    void togglePopup();

    void openPopup();

    void closePopup();

    ui::Rect getScreenBounds() const;

protected:
    bool handleLocalMouseMove(ControlLocalPointDp point) override;
    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override;

private:
    friend class ComboBoxPopup;

    void updatePopupBounds();

    int selectedIndex_ = -1;
    bool isOpen_ = false;
    std::string placeholder_ = "Select...";
    std::shared_ptr<ComboBoxPopup> popup_;
    std::shared_ptr<ComboTrigger> trigger_;
};

} // namespace ldt
