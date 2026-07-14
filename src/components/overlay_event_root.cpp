#include "overlay_event_root.h"

namespace ldt {

void OverlayEventRoot::onRender(DisplayList& displayList, const ui::Rect& clip) const {
    renderChildren(displayList, clip);
}

bool OverlayEventRoot::dispatchMouseMove(LogicalPointDp point) {
    bool handled = false;
    const auto& children = getChild();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        const auto& child = *it;
        if (!child || !child->isVisible()) continue;
        if (child->onLocalMouseMove(child->toLocalPoint(point))) {
            handled = true;
        }
    }
    return handled;
}

bool OverlayEventRoot::dispatchMouseButton(LogicalPointDp point, int button, int action) {
    const auto& children = getChild();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        const auto& child = *it;
        if (!child || !child->isVisible()) continue;
        if (child->onLocalMouseButton(child->toLocalPoint(point), button, action)) {
            return true;
        }
    }
    return false;
}

} // namespace ldt
