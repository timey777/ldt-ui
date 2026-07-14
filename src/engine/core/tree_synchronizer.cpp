#include "tree_synchronizer.h"

#include "engine/document_runtime.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_tree_view.h"
#include "engine/core/resolved_node.h"
#include "engine/core/ast_node.h"
#include "components/scene.h"
#include "components/control_manager.h"
#include "components/container_control.h"
#include "components/control_factory.h"
#include "components/image.h"
#include "components/input.h"
#include "engine/resource_manager.h"
#include "misc/logger.h"
#include "misc/perf_timer.h"

#include <unordered_map>
#include <functional>

namespace ldt {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

bool typesMatch(const std::shared_ptr<AbstractControl>& ctrl, ResolvedNode* rn) {
    if (!ctrl || !rn || !rn->astNode) return false;
    try {
        std::string ctrlType = ctrl->getTypeName();
        for (auto& c : ctrlType) c = static_cast<char>(::tolower(c));
        std::string nodeType = rn->astNode->type;
        for (auto& c : nodeType) c = static_cast<char>(::tolower(c));
        return ctrlType == nodeType;
    } catch (...) {
        LDT_ERROR("TreeSynchronizer: type check comparison failed");
        return false;
    }
}

void buildNodeUidMap(ResolvedNode* root,
                     std::unordered_map<std::string, ResolvedNode*>& nodeByUid,
                     bool skipRemoved = false) {
    if (!root) return;
    std::function<void(ResolvedNode*)> walk = [&](ResolvedNode* rn) {
        if (!rn || !rn->astNode) return;
        if (skipRemoved && rn->isDirty(DirtyFlag::Removed)) return;
        if (auto uidAttr = rn->astNode->getUid()) {
            try {
                if (uidAttr->isString()) {
                    const std::string uid = uidAttr->as<std::string>();
                    if (!nodeByUid.count(uid)) {
                        nodeByUid[uid] = rn;
                    } else {
                        LDT_ERROR("TreeSynchronizer: duplicate uid detected");
                    }
                }
            } catch (...) {
                LDT_ERROR("TreeSynchronizer: failed to read uid while building node map");
            }
        }
        for (auto* c : rn->getChildren()) walk(c);
    };
    walk(root);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// TreeSynchronizer::updateControl
// ---------------------------------------------------------------------------

void TreeSynchronizer::updateControl(const std::shared_ptr<AbstractControl>& ctrl, ResolvedNode* rn) {
    if (!ctrl || !rn) return;

    ControlFactory::getInstance()->BindControlToResolvedNode(rn, ctrl);
    ControlFactory::getInstance()->SyncPropertiesFromResolvedNode(rn, ctrl);

    // Image: update src and preload
    if (ctrl->getTypeName() == "Image") {
        auto imgCtrl = std::static_pointer_cast<Image>(ctrl);
        if (rn->astNode) {
            if (auto srcAttr = rn->astNode->getAttribute("src")) {
                try {
                    if (srcAttr->isString()) {
                        auto path = srcAttr->as<std::string>();
                        if (!path.empty()) {
                            imgCtrl->setSrc(path);
                            ResourceManager::getInstance().preloadImage(path);
                        }
                    }
                } catch (...) {
                    LDT_ERROR("TreeSynchronizer::updateControl: failed to update Image src");
                }
            }
        }
    }

    // Input: update text layout after bounds/viewport/scroll are synced
    if (ctrl->getTypeName() == "Input") {
        try {
            auto inputCtrl = std::dynamic_pointer_cast<Input>(ctrl);
            if (inputCtrl) {
                inputCtrl->updateTextLayout();
            }
        } catch (...) {
            LDT_ERROR("TreeSynchronizer::updateControl: failed to update Input text layout");
        }
    }

    rn->clearDirty(DirtyFlag::Style | DirtyFlag::Paint);
}

void TreeSynchronizer::updateControlLayoutOnly(const std::shared_ptr<AbstractControl>& ctrl, ResolvedNode* rn) {
    if (!ctrl || !rn) return;
    const auto& layout = rn->layout;
    ctrl->setBounds(layout.getBorderBox());
    ctrl->setScrollSize(layout.scroll.scrollWidth, layout.scroll.scrollHeight);
    ctrl->setViewportSize(layout.viewportWidth, layout.viewportHeight);
    ctrl->setScroll(layout.scroll.offsetX, layout.scroll.offsetY);
}

// ---------------------------------------------------------------------------
// TreeSynchronizer::syncLayoutOnly
// ---------------------------------------------------------------------------

void TreeSynchronizer::syncLayoutOnly(::DocumentRuntime* ctx, Scene* scene, const SyncScope& scope, bool layoutOnly) {
    if (!ctx || !scene || !scope.resolvedRoot) return;

    PerfWatch _w(layoutOnly ? "syncLayoutOnly(layout)" : "syncLayoutOnly(full)");

    std::unordered_map<std::string, ResolvedNode*> nodeByUid;
    buildNodeUidMap(scope.resolvedRoot, nodeByUid, /*skipRemoved=*/true);
    _w.split("buildMap");

    PerfCounter _pc;

    std::function<void(const std::shared_ptr<AbstractControl>&)> syncCtrl =
        [&](const std::shared_ptr<AbstractControl>& ctrl) {
        if (!ctrl) return;
        const std::string uid = ctrl->getUid();
        if (!uid.empty()) {
            auto it = nodeByUid.find(uid);
            if (it != nodeByUid.end() && typesMatch(ctrl, it->second)) {
                PerfCounter::Tick _t(_pc);
                if (layoutOnly)
                    updateControlLayoutOnly(ctrl, it->second);
                else
                    updateControl(ctrl, it->second);
            }
        }
        if (auto container = std::dynamic_pointer_cast<ContainerControl>(ctrl)) {
            for (const auto& ch : container->getChild()) syncCtrl(ch);
        }
    };

    if (scope.controlRoot) {
        syncCtrl(scope.controlRoot);
    } else {
        for (const auto& top : scene->getControlManager()->getControls()) {
            syncCtrl(top);
        }
    }

    _w.splitCounted("updateCtrls", _pc.count(), "ctls");
}

// ---------------------------------------------------------------------------
// TreeSynchronizer::syncToControls (full structural sync)
// ---------------------------------------------------------------------------

void TreeSynchronizer::syncToControls(::DocumentRuntime* ctx, Scene* scene, const SyncScope& scope) {
    if (!ctx || !scene || !scope.resolvedRoot) return;

    // 1) Build uid → ResolvedNode* map from the sync root
    std::unordered_map<std::string, ResolvedNode*> nodeByUid;
    buildNodeUidMap(scope.resolvedRoot, nodeByUid, /*skipRemoved=*/true);

    // 2) Collect existing controls; mark those missing from the resolved tree
    //    as pending deletion in a single pass.
    std::unordered_map<std::string, std::shared_ptr<AbstractControl>> controlByUid;
    std::vector<std::shared_ptr<AbstractControl>> pendingDeletion;

    std::function<void(const std::shared_ptr<AbstractControl>&)> collect =
        [&](const std::shared_ptr<AbstractControl>& ctrl) {
        if (!ctrl) return;
        const std::string uid = ctrl->getUid();
        if (!uid.empty()) {
            if (nodeByUid.count(uid)) {
                controlByUid[uid] = ctrl;
            } else {
                pendingDeletion.push_back(ctrl);
            }
        }
        if (auto container = std::dynamic_pointer_cast<ContainerControl>(ctrl)) {
            for (const auto& ch : container->getChild()) collect(ch);
        }
    };

    if (scope.controlRoot) {
        collect(scope.controlRoot);
    } else {
        for (const auto& top : scene->getControlManager()->getControls()) {
            collect(top);
        }
    }

    // 3) Update existing controls whose types still match
    for (const auto& kv : nodeByUid) {
        const std::string& uid = kv.first;
        ResolvedNode* rn = kv.second;
        if (!rn) continue;
        auto it = controlByUid.find(uid);
        if (it == controlByUid.end()) continue;

        auto ctrl = it->second;
        if (!typesMatch(ctrl, rn)) {
            pendingDeletion.push_back(ctrl);
            controlByUid.erase(it);
            continue;
        }

        updateControl(ctrl, rn);
    }

    // 4) Create new controls for resolved nodes missing from controlByUid
    std::unordered_map<std::string, std::shared_ptr<AbstractControl>> newlyCreated;
    for (const auto& kv : nodeByUid) {
        const std::string& uid = kv.first;
        ResolvedNode* rn = kv.second;
        if (!rn || controlByUid.count(uid) > 0) continue;

        try {
            auto newCtrl = ControlFactory::getInstance()->CreateControlFromResolvedNode(rn);
            if (!newCtrl) {
                LDT_ERROR("TreeSynchronizer::syncToControls: failed to create control for uid=" << uid);
                continue;
            }
            if (!rn->getControl().lock()) {
                LDT_ERROR("TreeSynchronizer::syncToControls: control not bound for uid=" << uid);
            }
            newlyCreated[uid] = newCtrl;
            controlByUid[uid] = newCtrl;
        } catch (...) {
            LDT_ERROR("TreeSynchronizer::syncToControls: exception creating control for uid=" << uid);
        }
    }

    // 5) Attach newly created controls to parent containers (or fallback)
    for (const auto& kv : newlyCreated) {
        const std::string& uid = kv.first;
        auto ctrl = kv.second;
        if (!ctrl) continue;

        ResolvedNode* rn = nodeByUid[uid];
        bool attached = false;

        if (rn && rn->parent && rn->parent->astNode) {
            try {
                if (auto parentUidAttr = rn->parent->astNode->getUid()) {
                    if (parentUidAttr->isString()) {
                        std::string parentUid = parentUidAttr->as<std::string>();
                        auto pit = controlByUid.find(parentUid);
                        if (pit != controlByUid.end() && pit->second) {
                            if (auto container = std::dynamic_pointer_cast<ContainerControl>(pit->second)) {
                                container->addChild(ctrl);
                                attached = true;
                            }
                        }
                    }
                }
            } catch (...) {
                LDT_ERROR("TreeSynchronizer::syncToControls: failed to attach child uid=" << uid);
            }
        }

        if (!attached) {
            if (scope.orphanContainer) {
                scope.orphanContainer->addChild(ctrl);
            } else {
                scene->getControlManager()->addRootControl(ctrl);
            }
        }
    }

    // 6) Remove stale controls (reverse order: children before parents)
    for (auto it = pendingDeletion.rbegin(); it != pendingDeletion.rend(); ++it) {
        auto ctrl = *it;
        if (!ctrl) continue;

        auto* parentRaw = ctrl->getParent();
        if (auto* parentContainer = dynamic_cast<ContainerControl*>(parentRaw)) {
            ContainerControl::InternalAccess::removeChild(parentContainer, ctrl);
        } else {
            scene->getControlManager()->removeRootControl(ctrl);
        }
    }
}

} // namespace ldt
