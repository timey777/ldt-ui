#include "combo_box.h"
#include "render/display_list.h"
#include "components/scene.h"
#include "components/control_manager.h"
#include "engine/ui_event_system.h"
namespace ldt {

ComboBox::ComboBox() {
    trigger_ = std::make_shared<ComboTrigger>(this);
    popup_ = std::make_shared<ComboBoxPopup>(this);
    
    addChild(trigger_);
}

void ComboBox::setOptions(const std::vector<std::string>& options) {
    for (int i = 0; i < (int)options.size(); ++i) {
        auto item = std::make_shared<ComboBoxItem>(options[i], i,
            [this](int idx) {
                this->setSelectedIndex(idx);
                this->closePopup();
            });
        popup_->addItem(item);
    }
    if (!options.empty() && selectedIndex_ < 0) {
        setSelectedIndex(0);
    }
}

void ComboBox::setSelectedIndex(int idx) {
    if (selectedIndex_ == idx) return;
    
    selectedIndex_ = idx;
    trigger_->setText(popup_->getItemText(idx));
    
    // 更新所有项的选中状态
    const auto& items = popup_->getItems();
    for (int i = 0; i < (int)items.size(); ++i) {
        items[i]->setSelected(i == idx);
    }

    // Dispatch Input event to UIEventSystem informing about selection change
    if (auto scene = getScene()) {
        auto ui = scene->getUIEventSystem();
        if (ui) {
            UIEventSystem::UIEvent e;
            e.type = UIEventSystem::UIEventType::Change;
            e.target = this;
            e.text = popup_->getItemText(idx);
            ui->dispatch(e);
        }
    }
}

void ComboBox::setBounds(const ui::Rect& b) {
    ContainerControl::setBounds(b);
    if (trigger_) trigger_->setBounds(b);
}

void ComboBox::onRender(DisplayList& displayList, const ui::Rect& clip) const {
    
    // Draw background
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

    // Render children
    renderChildren(displayList, clip);
}

void ComboBox::togglePopup() {
    if (isOpen_) closePopup();
    else openPopup();
}

ui::Rect ComboBox::getScreenBounds() const {
    ui::Rect rect = getBounds();

    for (const AbstractControl* current = this; current; current = current->getParent()) {
        rect.x += current->getVisualOffsetX();
        rect.y += current->getVisualOffsetY();

        const AbstractControl* parent = current->getParent();
        if (!parent) {
            continue;
        }

        rect.x -= parent->getScrollX();
        rect.y -= parent->getScrollY();
    }

    return rect;
}

void ComboBox::updatePopupBounds() {
    if (!popup_) {
        return;
    }

    constexpr float itemH = 30.0f;
    const auto rect = getScreenBounds();
    const float popupWidth = rect.width;
    const float popupHeight = popup_->getItems().size() * itemH;
    const float popupX = rect.x;
    const float popupY = rect.y + rect.height;

    popup_->setBounds(ui::Rect(popupX, popupY, popupWidth, popupHeight));

    const auto& items = popup_->getItems();
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        items[index]->setBounds(ui::Rect(popupX, popupY + index * itemH, popupWidth, itemH));
        items[index]->setSelected(index == selectedIndex_);
    }
}

void ComboBox::openPopup() {
    if (isOpen_) return;

    auto scene = getScene();
    if (!scene) return;

    isOpen_ = true;

    updatePopupBounds();
    popup_->setFillColor(ui::Color(1, 1, 1, 1));
    popup_->setStrokeColor(ui::Color(0.6f, 0.6f, 0.6f, 1));
    popup_->setStrokeWidth(ui::Edges(1.0f));

    getScene()->getControlManager()->addOverlayControl(popup_);
    invalidate();
}

void ComboBox::setTextColor(const ui::Color& color) {
    if (trigger_) trigger_->setTextColor(color);
}

void ComboBox::setFontSize(float size) {
    if (trigger_) trigger_->setFontSize(size);
}

void ComboBox::closePopup() {
    if (isOpen_ && popup_) {
        if (auto scene = getScene()) {
            scene->getControlManager()->removeOverlayControl(popup_);
        }
        invalidate();
        isOpen_ = false;
    }
}

bool ComboBox::handleLocalMouseMove(ControlLocalPointDp point)
{
    (void)point;
    return false;
}

bool ComboBox::handleLocalMouseButton(ControlLocalPointDp point, int button, int action)
{
    if (!trigger_ || !trigger_->isVisible()) return false;
    return trigger_->onLocalMouseButton(point, button, action);
}

} // namespace ldt
