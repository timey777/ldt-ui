#include "render/render_loop.h"

#include "application.h"

#include "engine/Window.h"
#include "render/graphics_context.h"
#include "engine/compositor.h"
#include "engine/view_coordinator.h"
#include "misc/perf_timer.h"
#include "components/scene.h"
#include "components/control_manager.h"
#include "components/control_factory.h"
#include "components/stage.h"
#include "engine/update_scheduler.h"

#include <thread>
#include <chrono>

namespace ldt {

RenderLoop::RenderLoop(Application& app)
    : app_(app) {}

RenderLoop::~RenderLoop() = default;

bool RenderLoop::runFrame()
{
    // 1) 事件轮询后：帧开始（叠加层可在此构建 UI）
    onFrameBegin();

    // 2) 推进动画
    onAnimate();

    // 3) 处理待定 resize（节流：每帧至多一次）
    onResize();

    // 4) 应用调度中的布局更新
    onApplyUpdates();

    // 5) 执行延迟移除的节点
    onExecuteRemovals();

    // 6) 若需要 present（脏状态或叠加层强制）则绘制并交换缓冲
    const bool dirty = app_.m_pendingPresent && app_.m_pendingPresent->exchange(false);
    if (dirty || onWantPresent())
    {
        app_.m_graphicsContext->clear(1, 1, 1, 1);
        if (app_.m_compositor) app_.m_compositor->paintAll();
        onFramePresent();   // 叠加层渲染点
        app_.m_graphicsContext->present();
        if (app_.m_window) app_.m_window->swapBuffers();
        return true;
    }

    // 7) 空闲路径：结束叠加层的一帧并让出 CPU
    onFrameEnd();
    onIdle();
    return false;
}

void RenderLoop::onShutdown() {}

Scene* RenderLoop::currentScene() const
{
    auto scene = Stage::getInstance().currentScene();
    return scene ? scene.get() : nullptr;
}

void RenderLoop::onFrameBegin() {}

void RenderLoop::onAnimate()
{
    Scene* currentScene = this->currentScene();
    if (currentScene && currentScene->getControlManager())
    {
        if (currentScene->getControlManager()->processAnimatedControls())
        {
            UpdateScheduler::getInstance().requestPaint();
        }
    }
}

void RenderLoop::onResize()
{
    if (!app_.m_pendingResize) return;
    app_.m_pendingResize = false;
    if (app_.m_host && app_.g_documentRuntime)
    {
        PerfWatch _w("resize");
        app_.m_host->handleResize(app_.m_pendingViewportSize);
    }
}

void RenderLoop::onApplyUpdates()
{
    auto updatePlan = UpdateScheduler::getInstance().consumePendingPlan();
    if (app_.m_host) app_.m_host->apply(updatePlan);
}

void RenderLoop::onExecuteRemovals()
{
    ControlFactory::getInstance()->executePendingRemovals();
}

bool RenderLoop::onWantPresent() const { return false; }

void RenderLoop::onFramePresent() {}

void RenderLoop::onFrameEnd() {}

void RenderLoop::onIdle()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

} // namespace ldt
