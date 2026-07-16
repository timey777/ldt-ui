#include "engine/view_coordinator.h"
#include "engine/document_runtime.h"
#include "engine/core/resolved_tree.h"
#include "components/scene.h"
#include "misc/perf_timer.h"

namespace ldt {

// ===================================================================
// ViewCoordinator — method implementations
// ===================================================================

void ViewCoordinator::apply(const DocumentUpdatePlan& plan) {
    if (!plan.hasWork()) return;
    switch (plan.kind) {
        case DocumentUpdateKind::ASTRepaint:   applyASTRepaint(); break;
        case DocumentUpdateKind::Repaint:
        case DocumentUpdateKind::FullLayout:    applyStyleUpdate(); break;
        case DocumentUpdateKind::PartialUpdate:
            applyPartialStyleUpdate(plan.dirtyNodes);
            return;
        case DocumentUpdateKind::PaintOnly:     applyPaintOnly(); break;
        case DocumentUpdateKind::None:          return;
    }
    requestPresent();
}

void ViewCoordinator::handleResize(ViewportSizeDp viewportSize) {
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

void ViewCoordinator::applyASTRepaint() {
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

void ViewCoordinator::applyStyleUpdate() {
    Scene* scene = resolveScene();
    if (!canRunPipeline(scene)) return;
    try { context_->runPipelineLayoutOnly(viewportSize_); } catch (...) {}
    SyncScope scope;
    scope.resolvedRoot = scene->getResolvedTree()->getRoot();
    if (scope.resolvedRoot) m_treeSynchronizer.syncLayoutOnly(context_, scene, scope);
    compositor_->renderScene(scene);
}

void ViewCoordinator::applyPartialStyleUpdate(const std::vector<ResolvedNode*>& nodes) {
    Scene* scene = resolveScene();
    if (!compositor_ || !context_ || !scene) return;

    for (auto* node : nodes) {
        if (!node) continue;
        if (auto ctrl = node->getControl().lock()) m_treeSynchronizer.updateControl(ctrl, node);
    }
    compositor_->renderScene(scene);
    requestPresent();
}

void ViewCoordinator::applyPaintOnly() {
    Scene* scene = resolveScene();
    if (!compositor_ || !scene) return;
    compositor_->renderScene(scene);
}

// ===== 构造 / 析构 =====

ViewCoordinator::ViewCoordinator(
    Compositor* compositor,
    DocumentRuntime* context,
    ViewportSizeDp initialViewportSize,
    std::atomic<bool>* pendingPresent,
    SceneResolver sceneResolver)
    : compositor_(compositor)
    , context_(context)
    , viewportSize_(initialViewportSize)
    , pendingPresent_(pendingPresent)
    , sceneResolver_(std::move(sceneResolver)) {
}

ViewCoordinator::~ViewCoordinator() = default;

// ===== 简单 setter =====

void ViewCoordinator::bind(Scene* scene) {
    boundScene_ = scene;
}

void ViewCoordinator::setViewport(ViewportSizeDp vp) {
    viewportSize_ = vp;
}

void ViewCoordinator::setScene(Scene* s) {
    boundScene_ = s;
}

void ViewCoordinator::setSceneResolver(SceneResolver sr) {
    sceneResolver_ = std::move(sr);
}

// ===== 内部工具 =====

Scene* ViewCoordinator::resolveScene() const {
    if (sceneResolver_) if (Scene* s = sceneResolver_()) return s;
    return boundScene_;
}

bool ViewCoordinator::hasViewport() const {
    return viewportSize_.width.value > 0.0f && viewportSize_.height.value > 0.0f;
}

bool ViewCoordinator::canRunPipeline(Scene* scene) const {
    return compositor_ && context_ && scene && hasViewport();
}

void ViewCoordinator::requestPresent() const {
    if (pendingPresent_) pendingPresent_->store(true);
}

} // namespace ldt
