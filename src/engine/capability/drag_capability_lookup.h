#pragma once

#include <vector>

#include "capability.h"

namespace ldt {

class ResolvedNode;

struct DragActivationTarget {
    ResolvedNode* node = nullptr;
    ::DraggableCap* cap = nullptr;
};

struct DropTargetMatch {
    ResolvedNode* node = nullptr;
    ::DropTargetCap* cap = nullptr;
};

DragActivationTarget findDragActivationTarget(const std::vector<ResolvedNode*>& path);
DropTargetMatch findDropTargetMatch(const std::vector<ResolvedNode*>& path, ResolvedNode* draggedNode);

} // namespace ldt