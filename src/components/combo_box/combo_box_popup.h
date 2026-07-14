#pragma once

#include "../container_control.h"
#include "combo_box_item.h"
#include <memory>

namespace ldt {

class ComboBox;

/**
 * @brief 下拉框弹出菜单容器
 */
class ComboBoxPopup : public ContainerControl {
public:
    explicit ComboBoxPopup(ComboBox* owner);
    
    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;
    
    std::string getTypeName() const override { return "ComboBoxPopup"; }
    
protected:
    bool handleLocalMouseMove(ControlLocalPointDp point) override;
    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override;

public:
    std::string getItemText(int idx) const;
    
    const std::vector<std::shared_ptr<ComboBoxItem>>& getItems() const { return items_; }
    
    void addItem(std::shared_ptr<ComboBoxItem> item);

private:
    ComboBox* owner_ = nullptr;
    std::vector<std::shared_ptr<ComboBoxItem>> items_;
};

} // namespace ldt
