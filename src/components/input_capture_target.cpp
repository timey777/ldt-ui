#include "input_capture_target.h"

#include "abstract_control.h"
#include "hit_test_helper.h"
#include "scrollbar_control.h"
#include "engine/core/resolved_node.h"

namespace {

ldt::ControlLocalPointDp toCapturedControlLocalPoint(ldt::ResolvedNode* targetSpace, ldt::LogicalPointDp point)
{
    return ldt::toControlLocalPoint(ldt::HitTestHelper::transformToNodeLocalPoint(targetSpace, point));
}

ldt::ControlLocalPointDp toCapturedControlLocalPoint(ldt::AbstractControl* targetSpace, ldt::LogicalPointDp point)
{
    return targetSpace->toLocalPoint(point);
}

ldt::ParentLocalPointDp toCapturedParentLocalPoint(ldt::AbstractControl* targetSpace, ldt::LogicalPointDp point)
{
    return ldt::toParentLocalPoint(toCapturedControlLocalPoint(targetSpace, point));
}

class ResolvedNodeCaptureTarget final : public ldt::IInputCaptureTarget {
public:
    ResolvedNodeCaptureTarget(ldt::ResolvedNode* node, ldt::ResolvedNode* relativeTo)
        : node_(node), relativeTo_(relativeTo) {}

    bool onMouseButton(ldt::LogicalPointDp point, int button, int action) override
    {
        if (!node_) return false;
        auto ctrl = node_->getControl().lock();
        if (!ctrl || !ctrl->isVisible()) return false;

        ldt::ResolvedNode* targetSpace = relativeTo_ ? relativeTo_ : node_;
        return ctrl->onLocalMouseButton(toCapturedControlLocalPoint(targetSpace, point), button, action);
    }

    void onMouseMove(ldt::LogicalPointDp point) override
    {
        if (!node_) return;
        auto ctrl = node_->getControl().lock();
        if (!ctrl || !ctrl->isVisible()) return;

        ldt::ResolvedNode* targetSpace = relativeTo_ ? relativeTo_ : node_;
        ctrl->onLocalMouseMove(toCapturedControlLocalPoint(targetSpace, point));
    }

    ldt::ResolvedNode* capturedNode() const override
    {
        return node_;
    }

    bool refersToNode(ldt::ResolvedNode* node) const override
    {
        return node_ == node || relativeTo_ == node;
    }

private:
    ldt::ResolvedNode* node_ = nullptr;
    ldt::ResolvedNode* relativeTo_ = nullptr;
};

class ControlCaptureTarget final : public ldt::IInputCaptureTarget {
public:
    ControlCaptureTarget(ldt::AbstractControl* control, ldt::AbstractControl* relativeTo)
        : control_(control), relativeTo_(relativeTo) {}

    bool onMouseButton(ldt::LogicalPointDp point, int button, int action) override
    {
        if (!control_ || !control_->isVisible()) return false;
        ldt::AbstractControl* targetSpace = relativeTo_ ? relativeTo_ : control_;
        if (relativeTo_) {
            if (auto* scrollbar = dynamic_cast<ldt::ScrollbarControl*>(control_)) {
                return scrollbar->onParentLocalMouseButton(toCapturedParentLocalPoint(targetSpace, point), button, action);
            }
        }
        return control_->onLocalMouseButton(toCapturedControlLocalPoint(targetSpace, point), button, action);
    }

    void onMouseMove(ldt::LogicalPointDp point) override
    {
        if (!control_ || !control_->isVisible()) return;
        ldt::AbstractControl* targetSpace = relativeTo_ ? relativeTo_ : control_;
        if (relativeTo_) {
            if (auto* scrollbar = dynamic_cast<ldt::ScrollbarControl*>(control_)) {
                scrollbar->onParentLocalMouseMove(toCapturedParentLocalPoint(targetSpace, point));
                return;
            }
        }
        control_->onLocalMouseMove(toCapturedControlLocalPoint(targetSpace, point));
    }

    ldt::ResolvedNode* capturedNode() const override
    {
        return nullptr;
    }

    bool refersToNode(ldt::ResolvedNode* node) const override
    {
        (void)node;
        return false;
    }

private:
    ldt::AbstractControl* control_ = nullptr;
    ldt::AbstractControl* relativeTo_ = nullptr;
};

} // namespace

namespace ldt {

std::unique_ptr<IInputCaptureTarget> createResolvedNodeCaptureTarget(ResolvedNode* node, ResolvedNode* relativeTo)
{
    return std::make_unique<ResolvedNodeCaptureTarget>(node, relativeTo);
}

std::unique_ptr<IInputCaptureTarget> createControlCaptureTarget(AbstractControl* control, AbstractControl* relativeTo)
{
    return std::make_unique<ControlCaptureTarget>(control, relativeTo);
}

} // namespace ldt
