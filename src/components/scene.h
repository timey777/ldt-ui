#pragma once

#include "components/abstract_control.h"
#include "components/container_control.h"
#include <algorithm>
#include "render/display_list.h"
#include "components/overlay_event_root.h"
#include <memory>
#include <vector>
#include <functional>
#include "engine/document_runtime.h"
#include "engine/ui_event_system.h"
#include <unordered_map>
#include <unordered_set>
#include <engine/core/resolved_tree.h>
#include "engine/core/resolved_node.h"
#include "engine/core/coordinate_types.h"
#include <engine/capability/drag_manager.h>
#include <engine/core/resolved_tree_view.h>
namespace ldt {

class ControlManager;
class IInputCaptureTarget;

/**
 * @brief 场景类 - 管理所有UI控件
 */
class LDT_API Scene {
    friend class ControlManager;
public:
    Scene() = delete;
    explicit Scene(::DocumentRuntime& runtime);
    virtual ~Scene(); // Need to define destructor in cpp for unique_ptr forward decl


    virtual bool setup();

    ::DocumentRuntime* getDocumentRuntime() const { return runtime_; }

    // ResolvedTree管理（Scene不拥有ResolvedTree，仅持有引用）
    //ResolvedTreeView* getResolvedTree()  { return &resolvedTree_; }
    ResolvedTreeView* getResolvedTreeView()  { return &resolvedTree_; }

    ResolvedTreeView* getResolvedTree()  { return &resolvedTree_; }

    // 便捷获取文本测量器（可能为空）
    ITextMeasurer* getTextMeasurer() const { return runtime_ ? runtime_->getTextMeasurer() : nullptr; }

    /**
     * @brief 渲染场景中的所有控件
     */
    virtual void render(DisplayList& displayList);

    ControlManager* getControlManager() const { return controlManager_.get(); }

    // 事件分发与焦点管理（高层 API）
    // 返回是否被处理
    bool onMouseButton(LogicalPointDp point, int button, int action);
    void onMouseMove(LogicalPointDp point);
    void onScroll(LogicalScrollDeltaDp delta);
    virtual void onKey(int key, int scancode, int action, int mods);
    void onChar(unsigned int codepoint);

    // 绑定便捷 API（将委托给 UIEventSystem）
    void onClick(const std::string& selector, UIEventSystem::EventCallback cb) {
        uiEventSystem_.bind(selector, UIEventSystem::UIEventType::Click, std::move(cb));
    }

    void onInput(const std::string& selector, UIEventSystem::EventCallback cb) {
        uiEventSystem_.bind(selector, UIEventSystem::UIEventType::Input, std::move(cb));
    }

    void onScroll(const std::string& selector, UIEventSystem::EventCallback cb) {
        uiEventSystem_.bind(selector, UIEventSystem::UIEventType::Scroll, std::move(cb));
    }

    // 请求重绘：暂时为空实现，后续由外部绑定具体行为
    void requestPaint() {}

    // 访问 Scene 中的 UIEventSystem
    UIEventSystem* getUIEventSystem() { return &uiEventSystem_; }

    // 节点被移除时的清理工作
    void notifyNodeRemoved(ResolvedNode* node);

    // Debug accessor: return last hit-test path (ResolvedNode* path)
    const std::vector<ResolvedNode*>& getDebugHitPath() const { return debugHitPath_; }


    /**
     * @brief 将 Scene 中运行时可持久化的数据（例如 Input 的 value）写回到 AST
     * @param root 全局 AST 的根节点（用于按 id 查找并写回）
     * @param overwrite 如果为 true 则强制覆盖 AST 中已存在的 value，否则仅在 AST 中不存在 value 时写回
     */
    void syncToAST(std::shared_ptr<ASTNode> root, bool overwrite = false);

    // 清除与指针相关的运行时状态（悬停/激活），例如当窗口失去焦点或鼠标在窗外释放时调用
    void clearPointerState();

    // 简单的鼠标捕获（仅在窗口内生效）：当一个控件需要接收全局鼠标事件时，调用 setMouseCapture
    void setMouseCapture(ResolvedNode* node, ResolvedNode* relativeTo = nullptr);
    void setMouseCaptureControl(AbstractControl* ctrl, AbstractControl* relativeTo = nullptr);
    void clearMouseCapture();
    ResolvedNode* getMouseCapture() const;

    // Cursor types (mirror of Window::CursorType). Use integers to avoid
    // coupling to Window header here.
    enum CursorType {
        Cursor_Arrow = 0,
        Cursor_Hand = 1,
        Cursor_IBeam = 2
    };

    // Request host to change cursor. Controls may call this during onMouseMove.
    void setCursor(int cursorType);

    // Apply the pending cursor state to the engine context if it differs from the active state.
    void flushCursor();

    // Clipboard helpers: delegate to Stage's IPlatformHost (shared across all scenes)
    void setClipboardString(const std::string& s);
    std::string getClipboardString() const;

private:
    std::unique_ptr<ControlManager> controlManager_;
    // 鼠标位置缓存
    LogicalPointDp lastMousePoint_;
    // 上次命中节点（用于判断 hover 是否变化，避免无效重绘）
    ResolvedNode* lastHitNode_ = nullptr;
    // 统一输入捕获目标（可由 ResolvedNode 或 AbstractControl 适配实现）
    std::unique_ptr<IInputCaptureTarget> mouseCaptureTarget_;
    ::DocumentRuntime* runtime_ = nullptr;

    // 解析树（非拥有，仅引用）
    ResolvedTreeView resolvedTree_;

    // Scene 自持有 UIEventSystem（原先在 DocumentRuntime 中）
    UIEventSystem uiEventSystem_; 
    DragManager   dragManager_;

    // 按下记录：保存哪些控件在按下时位于按下点（用于弹起时判定 click）
    std::unordered_set<AbstractControl*> pressedControls_;

    // 保存最近一次命中的控件路径（用于调试绘制），以 ResolvedNode* 表示
    std::vector<ResolvedNode*> debugHitPath_;

    int pendingCursor_ = Cursor_Arrow; 
    int activeCursor_ = Cursor_Arrow;
};

} // namespace ldt
