#include "combo_box_popup.h"
#include "combo_box.h"

namespace ldt {

ComboBoxPopup::ComboBoxPopup(ComboBox* owner)
    : owner_(owner) {}

void ComboBoxPopup::onRender(DisplayList& displayList, const ui::Rect& clip) const {
    if (owner_) {
        const_cast<ComboBox*>(owner_)->updatePopupBounds();
    }
    displayList.addRect(bounds_, fillColor_, strokeColor_, strokeWidth_, cornerRadius_);
    renderChildren(displayList, clip);
}

bool ComboBoxPopup::handleLocalMouseMove(ControlLocalPointDp point) {
    if (!visible_) return false;

    float globalX = point.x.value + bounds_.x;
    float globalY = point.y.value + bounds_.y;
    const LogicalPointDp globalPoint{ { globalX }, { globalY } };

    // 遍历所有子项，传递鼠标移动事件
    // 这样每个 item 都能正确更新其悬浮状态（进入或离开）
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (*it && (*it)->isVisible()) {
            (*it)->onLocalMouseMove((*it)->toLocalPoint(globalPoint));
        }
    }

    return hitTestLocal(point);
}

bool ComboBoxPopup::handleLocalMouseButton(ControlLocalPointDp point, int button, int action) {
    if (!visible_) return false;

    if (owner_) {
        owner_->updatePopupBounds();
    }

    float globalX = point.x.value + bounds_.x;
    float globalY = point.y.value + bounds_.y;
    const LogicalPointDp globalPoint{ { globalX }, { globalY } };

    // 遍历所有子项，传递鼠标按钮事件
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (*it && (*it)->isVisible()) {
            if ((*it)->onLocalMouseButton((*it)->toLocalPoint(globalPoint), button, action)) {
                return true;  // 事件被处理，停止传播
            }
        }
    }

    if (hitTestLocal(point)) {
        return true;  // 消费点击，防止穿透
    }

    if (action == 0 && owner_) {
        const auto ownerBounds = owner_->getScreenBounds();
        bool insideOwner = globalX >= ownerBounds.x && globalX <= ownerBounds.x + ownerBounds.width &&
                           globalY >= ownerBounds.y && globalY <= ownerBounds.y + ownerBounds.height;
        if (!insideOwner) {
            owner_->closePopup();
            return true;
        }
    }

    return false;
}

std::string ComboBoxPopup::getItemText(int idx) const {
    if (!items_.empty() && idx >= 0 && idx < (int)items_.size()) {
        return items_[idx]->getText();
    }
    return "";
}

void ComboBoxPopup::addItem(std::shared_ptr<ComboBoxItem> item) {
    if (item) {
        items_.push_back(item);
        addChild(item);
    }
}

} // namespace ldt
