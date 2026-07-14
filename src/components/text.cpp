#include "text.h"

#include "control_factory.h"
#include "engine/core/attribute.h"
#include "engine/core/resolved_node.h"

namespace ldt {

void Text::setText(const std::string& text) {
    if (text_ == text) {
        return;
    }

    text_ = text;
    layoutDirty_ = true;
    invalidate();

    const std::string& uid = getUid();
    if (uid.empty()) {
        return;
    }

    auto* factory = ControlFactory::getInstance();
    if (!factory) {
        return;
    }

    auto* resolvedNode = factory->FindNodeByUid(uid);
    if (!resolvedNode) {
        return;
    }

    if (resolvedNode->astNode) {
        resolvedNode->astNode->setAttribute("value", Attribute(text_));
    }

    resolvedNode->markDirty(DirtyFlag::Layout | DirtyFlag::Paint);
}

} // namespace ldt