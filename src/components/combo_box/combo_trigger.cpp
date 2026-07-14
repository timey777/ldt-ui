#include "combo_trigger.h"
#include "combo_box.h"
#include "render/display_list.h"

namespace ldt {

ComboTrigger::ComboTrigger(ComboBox* owner)
    : owner_(owner) {}

void ComboTrigger::setText(const std::string& text) {
    text_ = text;
}

void ComboTrigger::onRender(DisplayList& displayList, const ui::Rect& clip) const {
    auto bounds = owner_->getBounds();

    displayList.addRect(bounds, fillColor_, strokeColor_, strokeWidth_, cornerRadius_);
    auto txt = text_;
    if (txt.empty())
    {
        txt = "select a item";
    }
    if (!txt.empty()) {
        ui::Rect textBounds = bounds;
        textBounds.x += 8.0f;
        textBounds.width -= 24.0f;
        TextLayoutParams layout = TextLayoutParams();
        layout.wrap = false;
        displayList.addText(txt, textBounds, textColor_, fontSize_, "", layout);
    }

    ui::Rect arrowR(bounds.x + bounds.width - 16.0f, bounds.y, 16.0f, bounds.height);
    displayList.addText("v", arrowR, textColor_, fontSize_ - 2.0f);
}

bool ComboTrigger::handleLocalMouseButton(ControlLocalPointDp point, int button, int action) {
    if (!isVisible()) return false;
    if (button == 0 && action == 1) { // press
        if (hitTestLocal(point)) {
            owner_->togglePopup();
            return true;
        }
        owner_->closePopup();
    }
    return false;
}

bool ComboTrigger::handleLocalHitTest(ControlLocalPointDp point) const
{
    return point.x.value >= 0.0f && point.x.value <= owner_->getBounds().width
        && point.y.value >= 0.0f && point.y.value <= owner_->getBounds().height;
}

} // namespace ldt
