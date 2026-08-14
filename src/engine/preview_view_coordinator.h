#pragma once

#include "engine/view_coordinator.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_tree_view.h"
#include "engine/core/resolved_node_query.h"
#include "engine/document_runtime.h"
#include "engine/update_scheduler.h"
#include "components/container_control.h"
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
//   3. 引导：确保 preview 控件存在并沿解析树祖先链建好控件层级（TreeSynchronizer）
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

        // 引导：确保 preview 控件存在，并沿解析树祖先链建好控件层级
        //（控件树层级与解析树一致 → clip 正确传播、绘制顺序不被兄弟盖住）。
        // 具体由 TreeSynchronizer::ensureControlWithAncestors 负责。
        auto previewCtrl = m_treeSynchronizer.ensureControlWithAncestors(preview, scene);

        // Scoped structural sync
        SyncScope scope;
        scope.resolvedRoot    = preview;
        scope.controlRoot     = previewCtrl;
        scope.orphanContainer = dynamic_cast<ContainerControl*>(previewCtrl.get());

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
