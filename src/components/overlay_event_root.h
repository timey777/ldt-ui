#pragma once

#include "container_control.h"
#include "engine/core/coordinate_types.h"

namespace ldt {

class OverlayEventRoot : public ContainerControl {
public:
    OverlayEventRoot() = default;

    std::string getTypeName() const override { return "OverlayEventRoot"; }

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

    // Dispatch mouse events using absolute screen coordinates.
    bool dispatchMouseMove(LogicalPointDp point);
    bool dispatchMouseButton(LogicalPointDp point, int button, int action);

private:
};

} // namespace ldt
