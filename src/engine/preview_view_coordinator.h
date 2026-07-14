#pragma once

#include "engine/view_coordinator.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_tree_view.h"
#include "engine/core/resolved_node_query.h"
#include "engine/document_runtime.h"
#include "engine/update_scheduler.h"
#include "components/container_control.h"
#include "components/control_factory.h"
#include "components/control_manager.h"
#include "components/scene.h"
#include "misc/stable_id.h"
#include "misc/logger.h"

namespace ldt {

// ===================================================================
// PreviewViewCoordinator — 预览视图专用的 ViewCoordinator 子类
//
// 覆盖 applyASTRepaint()，在标准管道/Sync 之外增加：
//   1. 在 scene 的 ResolvedTree 中定位 preview 锚节点
//   2. 清空旧子树、挂载引擎 ResolvedTree
//   3. 确保 scene root 控件存在并将面板挂载为其子控件
//   4. 在 preview 范围内执行 syncToControls
//   5. 校验所有子节点均已绑定 Control
// ===================================================================
class LDT_API PreviewViewCoordinator : public ViewCoordinator {
public:
    explicit PreviewViewCoordinator(
        Compositor* compositor,
        DocumentRuntime* context,
        ViewportSizeDp initialViewportSize,
        std::atomic<bool>* pendingPresent = nullptr,
        SceneResolver sceneResolver = SceneResolver(),
        std::string previewSlotId = "preview")
        : ViewCoordinator(compositor, context, initialViewportSize, pendingPresent, std::move(sceneResolver))
        , previewSlotId_(std::move(previewSlotId)) {
    }

protected:
    void applyASTRepaint() override {
        Scene* scene = resolveScene();
        if (!scene) return;

        ResolvedTreeView* hostTree = scene->getResolvedTree();
        if (!hostTree) return;

        ResolvedNode* preview = ResolvedNodeQuery::FindById(hostTree, previewSlotId_.c_str());
        if (!preview) return;

        try { context_->runPipelineRenderTreeOnly(viewportSize_); }
        catch (...) { LDT_ERROR("PreviewViewCoordinator: runPipelineRenderTreeOnly threw"); }

        preview->clear();
        if (context_->getResolvedTree() && context_->getResolvedTree()->getRoot()) {
            hostTree->attachSubtreeFromOther(*context_->getResolvedTree(), preview);
        }

        // Ensure the preview panel itself has a control (needed as sync anchor).
        // Also ensure the scene root control exists so panels can be parented
        // under it — this is required for clip to propagate through the render
        // tree via the render() clip parameter during culling.
        auto previewCtrl = preview->getControl().lock();
        if (!previewCtrl) {
            try {
                previewCtrl = ControlFactory::getInstance()->CreateControlFromResolvedNode(preview);
                if (previewCtrl) {
                    auto* sceneRt = hostTree->getRoot();
                    std::shared_ptr<AbstractControl> sceneRootCtrl;
                    if (sceneRt) {
                        sceneRootCtrl = sceneRt->getControl().lock();
                        if (!sceneRootCtrl) {
                            sceneRootCtrl = ControlFactory::getInstance()->CreateControlFromResolvedNode(sceneRt);
                            if (sceneRootCtrl && !sceneRootCtrl->getParent()) {
                                scene->getControlManager()->addRootControl(sceneRootCtrl);
                            }
                        }
                    }
                    if (sceneRootCtrl) {
                        if (auto cc = std::dynamic_pointer_cast<ContainerControl>(sceneRootCtrl)) {
                            cc->addChild(previewCtrl);
                        }
                    }
                    if (!previewCtrl->getParent()) {
                        scene->getControlManager()->addRootControl(previewCtrl);
                    }
                }
            } catch (...) { LDT_ERROR("PreviewViewCoordinator: failed to create preview panel control"); }
        }

        // Scoped structural sync
        SyncScope scope;
        scope.resolvedRoot    = preview;
        scope.controlRoot     = preview->getControl().lock();
        scope.orphanContainer = dynamic_cast<ContainerControl*>(preview->getControl().lock().get());

        if (!preview->getChildren().empty()) {
            m_treeSynchronizer.syncToControls(context_, scene, scope);
        }

        // Validation: every resolved child must have a bound control
        try {
            std::function<void(ResolvedNode*)> check = [&](ResolvedNode* node) {
                if (!node) return;
                for (auto* child : node->getChildren()) {
                    if (!child) continue;
                    auto ctrl = child->getControl().lock();
                    if (!ctrl) {
                        std::string uid;
                        try {
                            if (child->astNode) {
                                if (auto uidAttr = child->astNode->getUid()) {
                                    if (uidAttr->isString()) uid = uidAttr->as<std::string>();
                                }
                            }
                        } catch (...) {}
                        std::string msg = "ResolvedNode child control is null";
                        if (!uid.empty()) msg += ", uid: " + uid;
                        LDT_ERROR(msg.c_str());
                    }
                    check(child);
                }
            };
            check(preview);
        } catch (...) {}

        UpdateScheduler::getInstance().requestRepaint();
        LDT_LOG("PreviewViewCoordinator applyASTRepaint");
    }

private:
    std::string previewSlotId_;
};

} // namespace ldt
