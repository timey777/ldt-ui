#pragma once

#include <memory>
#include "ldt_export.h"

// ldt-ui 中 Application 为全局类（非 ldt:: 命名空间），故在此做全局前向声明
class Application;

namespace ldt {

class Scene;

// ===================================================================
// RenderLoop —— 渲染流程抽象基类
//
// 将 Application 主循环中"每帧渲染流程"的固定顺序（模板方法）抽成独立抽象类：
//
//   事件轮询后 onFrameBegin
//     -> 动画推进 onAnimate
//     -> 待定 resize onResize
//     -> 布局更新 onApplyUpdates
//     -> 延迟移除 onExecuteRemovals
//     -> 需要 present：绘制 ldt 场景 -> onFramePresent（叠加层渲染点）-> 交换缓冲
//     -> 空闲：onFrameEnd + onIdle（默认让出 CPU）
//
// 子类（例如 preview 的 AppDelegate 内的 ImguiRenderLoop）只需覆写关心的步骤，
// 即可在固化流程上叠加额外渲染（如 Dear ImGui 属性检查器），无需改动 Application。
// ldt-ui 本身不感知任何具体叠加层（保持 imgui-free）。
// ===================================================================
class LDT_API RenderLoop {
public:
    explicit RenderLoop(::Application& app);
    virtual ~RenderLoop();

    RenderLoop(const RenderLoop&) = delete;
    RenderLoop& operator=(const RenderLoop&) = delete;

    // 模板方法：执行一帧。返回 true 表示本帧发生了 present（已交换缓冲）。
    // 由 Application 主循环每帧调用。
    bool runFrame();

    // 应用退出、图形上下文关闭前调用（用于叠加层清理）。
    virtual void onShutdown();

protected:
    ::Application& app() const { return app_; }

    // 当前活动场景（可能为 nullptr）。
    Scene* currentScene() const;

    // ---- 每帧步骤（子类可覆写） ----
    // 事件轮询后、处理脏状态前调用（叠加层可在此 newFrame + 构建 UI）。
    virtual void onFrameBegin();
    // 推进动画。
    virtual void onAnimate();
    // 处理待定 resize（节流：每帧至多一次）。
    virtual void onResize();
    // 应用调度中的布局更新。
    virtual void onApplyUpdates();
    // 执行延迟移除的节点。
    virtual void onExecuteRemovals();
    // 返回 true 表示即使无脏状态也强制本帧 present（如叠加层需要持续绘制）。
    virtual bool onWantPresent() const;
    // 在 ldt 场景绘制完成后、交换缓冲前调用（叠加层渲染点）。
    virtual void onFramePresent();
    // 本帧未 present（空闲路径）时调用（叠加层可在此正确结束一帧）。
    virtual void onFrameEnd();
    // 空闲处理（默认 sleep 让出 CPU）。
    virtual void onIdle();

private:
    ::Application& app_;
};

// 默认渲染循环：保持 Application 原有的固化流程（无任何叠加层）。
class LDT_API DefaultRenderLoop : public RenderLoop {
public:
    using RenderLoop::RenderLoop;
};

} // namespace ldt
