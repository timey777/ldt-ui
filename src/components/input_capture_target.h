#pragma once

#include <memory>

#include "engine/core/coordinate_types.h"

namespace ldt {
class AbstractControl;
class ResolvedNode;

class IInputCaptureTarget {
public:
    virtual ~IInputCaptureTarget() = default;
    virtual bool onMouseButton(LogicalPointDp point, int button, int action) = 0;
    virtual void onMouseMove(LogicalPointDp point) = 0;
    virtual ResolvedNode* capturedNode() const = 0;
    virtual bool refersToNode(ResolvedNode* node) const = 0;
};

std::unique_ptr<IInputCaptureTarget> createResolvedNodeCaptureTarget(ResolvedNode* node, ResolvedNode* relativeTo);
std::unique_ptr<IInputCaptureTarget> createControlCaptureTarget(AbstractControl* control, AbstractControl* relativeTo);

} // namespace ldt
