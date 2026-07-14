#include "drag_manager.h"
#include "components/abstract_control.h"
#include "capability.h"
#include "drag_capability_lookup.h"
#include "engine/event_dispatcher.h"
#include "components/scene.h"
#include "engine/core/resolved_node.h"

namespace ldt {
    static DropTargetEvent makeDropTargetEvent(ResolvedNode* draggedNode, LogicalPointDp hitPoint)
    {
        return { draggedNode, hitPoint };
    }

    static void notifyHoveredDropTarget(DragState& state, DropTargetMatch dropTarget, LogicalPointDp hitPoint)
    {
        if (state.hoveredDropTarget == dropTarget.node) {
            return;
        }
        state.hoveredDropTarget = dropTarget.node;
        if (!dropTarget.cap) {
            return;
        }
        if (dropTarget.cap->onHover) {
            dropTarget.cap->onHover(makeDropTargetEvent(state.node, hitPoint));
        }
    }

    bool DragManager::onMouseButton(LogicalPointDp point, int button, int action)
    { 
        if (action == 0) { // mouse up
            if (active_) {
                if (active_->dragging && active_->cap) {
                    auto hit = HitTestHelper::performHitTest(scene_->getResolvedTreeView(), point);
                    const DropTargetMatch dropTarget = findDropTargetMatch(hit.path, active_->node);
                    auto ctrl = active_->node->getControl().lock();
                    if (ctrl) ctrl->onDrop();
                    if (dropTarget.cap && dropTarget.cap->onDrop) {
                        dropTarget.cap->onDrop(makeDropTargetEvent(active_->node, point));
                    }
                }
                active_.reset();
                return true;
            }
        }
        // 2️⃣ 还没 capture，尝试启动 drag
        if (button != 0 || action != 1)
            return false;

        auto hit = HitTestHelper::performHitTest(scene_->getResolvedTreeView(), point);
        if (!hit.hitNode) return false;

        const DragActivationTarget dragTarget = findDragActivationTarget(hit.path);
        if (!dragTarget.node || !dragTarget.cap) return false;

        active_ = DragState();
        active_->node = dragTarget.node;
        active_->cap = dragTarget.cap;
        active_->control = dragTarget.node->getControl().lock().get();
        active_->startPos = point;
        active_->dragging = false;
        return true; // mouse down 被 DragManager 接管
    }
    bool DragManager::onMouseMove(LogicalPointDp point)
    {
        if (!active_) return false;

        const LogicalDragOffsetDp delta = makeLogicalDragOffset(
            point.x.value - active_->startPos.x.value,
            point.y.value - active_->startPos.y.value);

        if (!active_->dragging) {
            //if (length(delta) < 3.0f) {
            //    return true; // 还没到拖动阈值
            //}

            active_->dragging = true;
            //setCapture(active_->control);
            active_->control->onDragStart();
        }

        auto hit = HitTestHelper::performHitTest(scene_->getResolvedTreeView(), point);
        notifyHoveredDropTarget(*active_, findDropTargetMatch(hit.path, active_->node), point);
        active_->control->onLogicalDragMove(delta);
        return true;
    }

    void DragManager::cancel()
    {
        active_.reset();
    }


} // namespace ldt
