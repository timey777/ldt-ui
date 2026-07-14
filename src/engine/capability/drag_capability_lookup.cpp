#include "drag_capability_lookup.h"

#include "engine/core/resolved_node.h"

namespace ldt {

namespace {

bool isSameOrDescendantOf(ResolvedNode* node, ResolvedNode* ancestor)
{
    for (ResolvedNode* current = node; current; current = current->parent) {
        if (current == ancestor) {
            return true;
        }
    }
    return false;
}

} // namespace

DragActivationTarget findDragActivationTarget(const std::vector<ResolvedNode*>& path)
{
    DragActivationTarget result;

    for (ResolvedNode* node : path) {
        if (!node) continue;

        if (node->caps().get<BlockDragCap>()) {
            return {};
        }

        if (auto* handle = node->caps().get<DragHandleCap>()) {
            ResolvedNode* owner = handle->owner;
            if (!owner) {
                continue;
            }

            auto* cap = owner->caps().get<DraggableCap>();
            if (cap && cap->enabled) {
                result = { owner, cap };
            }
        }
    }

    return result;
}

DropTargetMatch findDropTargetMatch(const std::vector<ResolvedNode*>& path, ResolvedNode* draggedNode)
{
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        ResolvedNode* node = *it;
        if (!node) continue;
        if (isSameOrDescendantOf(node, draggedNode)) continue;

        if (auto* cap = node->caps().get<DropTargetCap>()) {
            return { node, cap };
        }
    }

    return {};
}

} // namespace ldt