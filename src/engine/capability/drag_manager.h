#pragma once
#include <memory>
#include "engine/core/coordinate_types.h"
#include "engine/core/resolved_node.h"
#include <engine/ui_event_system.h>

namespace ldt {
    class Scene;
    struct DragState {
        AbstractControl* control = nullptr;
        ldt::ResolvedNode* node = nullptr;
        Capability* cap = nullptr;
        LogicalPointDp startPos;
        ldt::ResolvedNode* hoveredDropTarget = nullptr;
        bool dragging = false;
    };
    class DragManager {
    public:
        explicit DragManager(Scene* scene)
            : scene_(scene) {
        }

        bool onMouseButton(LogicalPointDp point, int button, int action);
        bool onMouseMove(LogicalPointDp point);
        void cancel();

    private:
        Scene* scene_ = nullptr;

        std::optional<DragState> active_;

    private:
    };

} // namespace ldt
