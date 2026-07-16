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
        SceneResolver sceneResolver = SceneResolver());

    ~ViewCoordinator();

    void bind(Scene* scene);

    // ===== 核心入口 (virtual) =====

    virtual void apply(const DocumentUpdatePlan& plan);
    virtual void handleResize(ViewportSizeDp viewportSize);

    void setViewport(ViewportSizeDp vp);
    void setScene(Scene* s);
    void setSceneResolver(SceneResolver sr);

protected:
    // ===== 子类可覆盖的更新步骤 =====

    virtual void applyASTRepaint();
    virtual void applyStyleUpdate();
    virtual void applyPartialStyleUpdate(const std::vector<ResolvedNode*>& nodes);
    virtual void applyPaintOnly();

    // ===== 内部工具 =====

    Scene* resolveScene() const;
    bool hasViewport() const;
    bool canRunPipeline(Scene* scene) const;
    void requestPresent() const;

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

