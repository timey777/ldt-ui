#pragma once

#include <memory>
#include <vector>
#include "ldt_export.h"

class DocumentRuntime;

namespace ldt {
    class Scene;
    class ResolvedNode;
    class AbstractControl;
    class ContainerControl;
}

namespace ldt {

// Anchor points for syncing a resolved-tree subtree to UI controls.
// The same algorithm is reused by root-level Compositor and preview-level
// Compositors — only the anchor points differ.
struct LDT_API SyncScope {
    // Walk the resolved tree starting from this node (required).
    ResolvedNode* resolvedRoot = nullptr;

    // Collect existing controls starting from this control.
    // When nullptr, controls are collected from Scene top-level via ControlManager.
    std::shared_ptr<AbstractControl> controlRoot;

    // Where to attach newly-created orphan controls that have no parent control.
    // When nullptr, orphans are added to the Scene root via ControlManager.
    ContainerControl* orphanContainer = nullptr;
};

// Composable utility that synchronizes a Scene's control tree with a
// ResolvedTree subtree.  Callers own an instance and call methods as needed;
// the class itself is stateless today but leaves room for caching / incremental
// diff state in the future.
class LDT_API TreeSynchronizer {
public:
    TreeSynchronizer() = default;
    ~TreeSynchronizer() = default;

    // Full structural sync: create, update, and delete controls so the Scene
    // control tree matches the resolved-tree subtree defined by `scope`.
    void syncToControls(::DocumentRuntime* ctx, Scene* scene, const SyncScope& scope);

    // Layout/style-only sync: update bounds and styles on *existing* controls
    // only.  No controls are created or deleted.  Suitable for resize /
    // style-only changes.
    // When layoutOnly=true, uses lightweight bounds/scroll sync (resize).
    // When layoutOnly=false, uses full property sync (style repaints).
    void syncLayoutOnly(::DocumentRuntime* ctx, Scene* scene, const SyncScope& scope, bool layoutOnly = false);

    // Update a single control from its resolved node (bind + sync properties +
    // Image src / Input text-layout special handling).  Also used by
    // handlePartialStyleUpdate.
    void updateControl(const std::shared_ptr<AbstractControl>& ctrl, ResolvedNode* rn);

    // Layout-only lightweight update: bounds, scroll, viewport only.
    // Skips BindControlToResolvedNode, style properties, and type-specific
    // handling (Image/Input). Suitable for resize where only layout changed.
    void updateControlLayoutOnly(const std::shared_ptr<AbstractControl>& ctrl, ResolvedNode* rn);
};

} // namespace ldt
