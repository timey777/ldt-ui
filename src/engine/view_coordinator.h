#pragma once

#include "compositor.h"
#include "core/tree_synchronizer.h"
#include "document_update_plan.h"
#include "core/coordinate_types.h"
#include "core/node_flags.h"
#include "update_scheduler.h"
#include "misc/perf_timer.h"

#include <atomic>
#include <functional>
#include <utility>

class DocumentRuntime;

namespace ldt {

// ===================================================================
// ViewCoordinator — 单视图更新协调器（基类，不再 final）
//
// 持有 TreeSynchronizer，根据 DocumentUpdatePlan 直接执行：
//   管道运行 → ResolvedTree 合并 → Control 同步 → Compositor::renderScene
//
// apply() / handleResize() / applyASTRepaint() 等均为 virtual，
// 子类可覆盖以自定义预览/嵌入式视图的更新流程。
// ===================================================================
class LDT_API ViewCoordinator {
public:
    using SceneResolver = std::function<Scene*()>;

    ViewCoordinator(
        Compositor* compositor,
        DocumentRuntime* context,
        ViewportSizeDp initialViewportSize,
        std::atomic<bool>* pendingPresent = nullptr,
        SceneResolver sceneResolver = SceneResolver())
        : compositor_(compositor)
        , context_(context)
        , viewportSize_(initialViewportSize)
        , pendingPresent_(pendingPresent)
        , sceneResolver_(std::move(sceneResolver)) {
    }

    virtual ~ViewCoordinator() = default;

    void bind(Scene* scene) { boundScene_ = scene; }

    // ===== 核心入口 (virtual) =====

    virtual void apply(const DocumentUpdatePlan& plan) {
        if (!plan.hasWork()) return;
        switch (plan.kind) {
            case DocumentUpdateKind::ASTRepaint:   applyASTRepaint(); break;
            case DocumentUpdateKind::Repaint:
            case DocumentUpdateKind::FullLayout:    applyStyleUpdate(); break;
            case DocumentUpdateKind::PartialUpdate: applyPartialStyleUpdate(plan.dirtyNodes); return;
            case DocumentUpdateKind::PaintOnly:     applyPaintOnly(); break;
            case DocumentUpdateKind::None:          return;
        }
        requestPresent();
    }

    virtual void handleResize(ViewportSizeDp viewportSize) {
        PerfWatch _w("handleResize");
        viewportSize_ = viewportSize;
        Scene* scene = resolveScene();
        if (!canRunPipeline(scene)) return;
        if (auto rt = context_->getResolvedTree()) {
            if (auto root = rt->getRoot()) {
                root->markDirtyNoCascade(DirtyFlag::Layout);
            }
        }
        _w.split("pre");

        try { context_->runPipelineLayoutOnly(viewportSize_); } catch (...) {}
        _w.split("pipeline");

        SyncScope scope;
        scope.resolvedRoot = scene->getResolvedTree()->getRoot();
        if (scope.resolvedRoot) m_treeSynchronizer.syncLayoutOnly(context_, scene, scope, /*layoutOnly=*/true);
        _w.split("sync");

        compositor_->renderScene(scene);
        _w.split("render");

        requestPresent();
    }

    void setViewport(ViewportSizeDp vp) { viewportSize_ = vp; }
    void setScene(Scene* s) { boundScene_ = s; }
    void setSceneResolver(SceneResolver sr) { sceneResolver_ = std::move(sr); }

protected:
    // ===== 子类可覆盖的更新步骤 =====

    virtual void applyASTRepaint() {
        Scene* scene = resolveScene();
        if (!canRunPipeline(scene)) return;
        try { context_->runPipelineRenderTreeOnly(viewportSize_); } catch (...) {}
        // Merge engine tree into scene tree on first build
        {
            auto* rt = scene->getResolvedTree();
            if (rt && context_->getResolvedTree() && context_->getResolvedTree()->getRoot()) {
                if (!rt->getRoot()) rt->attachSubtreeFromOther(*context_->getResolvedTree(), nullptr, false);
            }
        }
        SyncScope scope;
        scope.resolvedRoot = scene->getResolvedTree()->getRoot();
        if (scope.resolvedRoot) m_treeSynchronizer.syncToControls(context_, scene, scope);
        compositor_->renderScene(scene);
    }

    virtual void applyStyleUpdate() {
        Scene* scene = resolveScene();
        if (!canRunPipeline(scene)) return;
        try { context_->runPipelineLayoutOnly(viewportSize_); } catch (...) {}
        SyncScope scope;
        scope.resolvedRoot = scene->getResolvedTree()->getRoot();
        if (scope.resolvedRoot) m_treeSynchronizer.syncLayoutOnly(context_, scene, scope);
        compositor_->renderScene(scene);
    }

    virtual void applyPartialStyleUpdate(const std::vector<ResolvedNode*>& nodes) {
        Scene* scene = resolveScene();
        if (!compositor_ || !context_ || !scene) return;
        for (auto* node : nodes) {
            if (!node) continue;
            if (auto ctrl = node->getControl().lock()) m_treeSynchronizer.updateControl(ctrl, node);
        }
        compositor_->renderScene(scene);
        requestPresent();
    }

    virtual void applyPaintOnly() {
        Scene* scene = resolveScene();
        if (!compositor_ || !scene) return;
        compositor_->renderScene(scene);
    }

    // ===== 内部工具 =====

    Scene* resolveScene() const {
        if (sceneResolver_) if (Scene* s = sceneResolver_()) return s;
        return boundScene_;
    }
    bool hasViewport() const { return viewportSize_.width.value > 0.0f && viewportSize_.height.value > 0.0f; }
    bool canRunPipeline(Scene* scene) const { return compositor_ && context_ && scene && hasViewport(); }
    void requestPresent() const { if (pendingPresent_) pendingPresent_->store(true); }

    // ===== 成员 (protected 以便子类访问) =====

    TreeSynchronizer   m_treeSynchronizer;
    Compositor*        compositor_   = nullptr;
    DocumentRuntime*     context_      = nullptr;
    ViewportSizeDp     viewportSize_{};

private:
    Scene*             boundScene_   = nullptr;
    std::atomic<bool>* pendingPresent_ = nullptr;
    SceneResolver      sceneResolver_;
};

} // namespace ldt

