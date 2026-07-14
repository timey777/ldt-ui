#include "scene.h"
#include "components/stage.h"
#include "control_manager.h"
#include "hit_test_helper.h"
#include "engine/event_dispatcher.h"
#include "engine/ui_event_system.h"
#include "components/overlay_event_root.h"
#include <algorithm>
#include <iostream>
#include "engine/input_events.h"
#include "engine/core/parse.h"
#include "input.h"
#include "input_capture_target.h"
#include "engine/core/ast_node.h"
#include "engine/core/resolved_node.h"
#include "engine/update_scheduler.h"
#include "misc/ui_diagnostics.h"
#include "misc/logger.h"

namespace ldt
{
    Scene::Scene(DocumentRuntime& runtime)
        : runtime_(&runtime)
        , dragManager_(this) {
        controlManager_ = std::make_unique<ControlManager>(this);
    }
    
    Scene::~Scene() = default;

    bool Scene::setup()
    {
        return false;
    }


    void Scene::render(DisplayList& displayList)
    {
        if (controlManager_) controlManager_->render(displayList);
    }
    
    bool Scene::onMouseButton(LogicalPointDp point, int button, int action)
    {
        bool handled = false;
        auto* ui = getUIEventSystem();

        if (ui && action == 0)
        {
            ui->updateActive(nullptr);
        }

        // 如果有捕获，直接发送给捕获者
        if (mouseCaptureTarget_)
        {
            handled = mouseCaptureTarget_->onMouseButton(point, button, action);
            // 释放时清除捕获
            if (action == 0) 
            {
                clearMouseCapture();
            }
            if (handled) {
                if (action == 0)
                {
                    pressedControls_.clear();
                }
                return true;
            }
        }
        else 
        {
            if (dragManager_.onMouseButton(point, button, action)) {
                if (action == 0)
                {
                    pressedControls_.clear();
                }
                return true;
            }
        }

        if (controlManager_ && controlManager_->getOverlayRoot() && controlManager_->getOverlayRoot()->dispatchMouseButton(point, button, action)) {
            if (action == 0) {
                pressedControls_.clear();
            }
            return true;
        }

        // 1. 执行命中测试
        auto hitRes = HitTestHelper::performHitTest(getResolvedTreeView(), point);
        // 保存用于调试绘制的路径
        debugHitPath_ = hitRes.path;
        diagnostics::logHitResult("scene.onMouseButton.pre", point, button, action, hitRes.path, hitRes.hitNode);

        // 2. 更新Active和Focus状态
        if (ui)
        {
            if (action == 1) // 按下
            {
                ui->updateActive(hitRes.hitNode);
                ui->updateFocus(hitRes.hitNode);
            }
        }

        // 3. 分发事件到路径上的控件
        bool pathHandled = EventDispatcher::dispatchMouseButton(
            hitRes.path, point, button, action, ui, &pressedControls_);

        // 4. 清理按下记录
        if (action == 0)
        {
            pressedControls_.clear();
        }

        diagnostics::logSceneUiState("scene.onMouseButton.post", ui, getMouseCapture());

        return handled || pathHandled;
    }

    void Scene::onMouseMove(LogicalPointDp point)
    {
        lastMousePoint_ = point;

        // 重置光标
        setCursor(CursorType::Cursor_Arrow);

        // 如果有捕获，直接发送给捕获者
        if (mouseCaptureTarget_)
        {
            mouseCaptureTarget_->onMouseMove(point);
            flushCursor();
            return;
        }

        if (dragManager_.onMouseMove(point)) {
            flushCursor();
            return;
        }

        if (controlManager_ && controlManager_->getOverlayRoot()) {
            bool overlayHandled = controlManager_->getOverlayRoot()->dispatchMouseMove(point);
            if (overlayHandled) {
                flushCursor();
                return;
            }
        }

        // 执行命中测试
        auto hitRes = HitTestHelper::performHitTest(getResolvedTreeView(), point);
        // 保存用于调试绘制的路径
        debugHitPath_ = hitRes.path;
        diagnostics::logHitResult("scene.onMouseMove.pre", point, -1, -1, hitRes.path, hitRes.hitNode);
        // 仅当 hover 目标变化时才请求重绘，避免无效渲染
        if (hitRes.hitNode != lastHitNode_) {
            UpdateScheduler::getInstance().requestPaint();
        }
        lastHitNode_ = hitRes.hitNode;
        // 如果没有命中任何控件，直接返回
        if (!hitRes.hitNode && hitRes.path.empty())
        {
            flushCursor();
            return;
        }

        // 更新Hover状态
        auto* ui = getUIEventSystem();
        if (ui)
        {
            ui->updateHover(hitRes.hitNode);
        }

        // 分发事件
        EventDispatcher::dispatchMouseMove(hitRes.path, point);
        diagnostics::logSceneUiState("scene.onMouseMove.post", ui, getMouseCapture());
        flushCursor();
    }

    void Scene::onScroll(LogicalScrollDeltaDp delta)
    {
        // 使用命中测试定位当前指针下的控件路径，按路径从内到外分发滚动事件
        auto hitRes = HitTestHelper::performHitTest(getResolvedTreeView(), lastMousePoint_);
        // 保存用于调试绘制的路径
        debugHitPath_ = hitRes.path;

        // 从最深层开始向上尝试分发滚动事件
        for (int i = static_cast<int>(hitRes.path.size()) - 1; i >= 0; --i) {
            ResolvedNode* node = hitRes.path[i];
            if (!node) continue;
            auto ctrl = node->getControl().lock();
            if (!ctrl || !ctrl->isVisible()) continue;
            if (ctrl->onLocalScroll({ delta.dx, delta.dy })) {
                return;
            }
        }
    }

    void Scene::onKey(int key, int scancode, int action, int mods)
    {
        auto *focusNode = getUIEventSystem()->getFocusNode();
        if (focusNode && focusNode->getControl().lock())
        {
            focusNode->getControl().lock()->onKey(key, scancode, action, mods);
        }
    }

    void Scene::onChar(unsigned int codepoint)
    {
        auto *focusNode = getUIEventSystem()->getFocusNode();
        if (focusNode && focusNode->getControl().lock())
        {
            focusNode->getControl().lock()->onChar(codepoint);
        }
    }



    // Animated controls management - REMOVED (Moved to ControlManager)


    void Scene::syncToAST(std::shared_ptr<ASTNode> root, bool overwrite)
    {
        if (!root)
            return;

        // helper: find AST node by internal uid (DFS)
        std::function<std::shared_ptr<ASTNode>(const std::shared_ptr<ASTNode> &, const std::string &)> findByUid;
        findByUid = [&](const std::shared_ptr<ASTNode> &node, const std::string &uid) -> std::shared_ptr<ASTNode>
        {
            if (!node)
                return nullptr;
            try
            {
                if (auto a = node->getUid())
                {
                    if (a->isString() && a->as<std::string>() == uid)
                        return node;
                }
            }
            catch (...)
            {
                LDT_ERROR("Scene::syncToAST: findByUid failed");
            }
            for (const auto &c : node->children)
            {
                auto r = findByUid(c, uid);
                if (r)
                    return r;
            }
            return nullptr;
        };

        std::function<void(const std::shared_ptr<AbstractControl> &)> walk;
        walk = [&](const std::shared_ptr<AbstractControl> &ctrl)
        {
            if (!ctrl)
                return;
            try
            {
                // use control's internal uid for stable matching against AST's uid
                auto& uid = ctrl->getUid();
                if (!uid.empty())
                {
                    auto astNode = findByUid(root, uid);
                    if (astNode)
                    {
                        // Scene-level persistence: controls remain pure UI; Scene inspects
                        // known control types and writes runtime state into AST.
                        try
                        {
                            if (auto inputCtrl = std::dynamic_pointer_cast<ldt::Input>(ctrl))
                            {
                                bool shouldWrite = overwrite;
                                try
                                {
                                    if (!astNode->hasAttribute("value"))
                                        shouldWrite = true;
                                    else if (astNode->getAttribute("value"))
                                    {
                                        if (astNode->getAttribute("value")->isString())
                                        {
                                            const std::string v = astNode->getAttribute("value")->as<std::string>();
                                            if (v.empty())
                                                shouldWrite = true;
                                        }
                                    }
                                }
                                catch (...)
                                {
                                    LDT_ERROR("Scene::syncToAST: failed to check value attribute");
                                }

                                if (shouldWrite)
                                {
                                    astNode->attrs["value"] = Attribute(inputCtrl->getValue());
                                }
                            }
                            // TODO: handle other control types (checkbox/select/etc.) here
                        }
                        catch (...)
                        {
                            LDT_ERROR("Scene::syncToAST: failed to sync control state");
                        }
                    }
                }
            }
            catch (...)
            {
                LDT_ERROR("Scene::syncToAST: walk iteration failed");
            }

            if (auto container = std::dynamic_pointer_cast<ContainerControl>(ctrl))
            {
                for (const auto &c : container->getChild())
                    walk(c);
            }
        };

        if (controlManager_) {
             for (const auto &top : controlManager_->getControls())
                 walk(top);
        }
    }

    // 清除与指针相关的运行时状态（悬停/激活）
    void Scene::clearPointerState()
    {
        pressedControls_.clear();
        clearMouseCapture();
        dragManager_.cancel();
        lastHitNode_ = nullptr;

        // Clear scene UIEventSystem state
        auto *ui = getUIEventSystem();
        if (ui)
        {
            ui->updateActive(nullptr);
            ui->updateHover(nullptr);
        }

        pendingCursor_ = Cursor_Arrow;
        flushCursor();
    }

    // Request host to change cursor (controls call this during onMouseMove)
    void Scene::setCursor(int cursorType)
    {
        pendingCursor_ = cursorType;
    }

    void Scene::flushCursor()
    {
        if (pendingCursor_ != activeCursor_)
        {
            Stage::getInstance().setCursor(pendingCursor_);
            activeCursor_ = pendingCursor_;
        }
    }

    void Scene::setClipboardString(const std::string& s) {
        Stage::getInstance().setClipboardString(s);
    }

    std::string Scene::getClipboardString() const {
        return Stage::getInstance().getClipboardString();
    }

    // Simple mouse capture implementation
    void Scene::setMouseCapture(ResolvedNode *ctrl, ResolvedNode* relativeTo)
    {
        if (!ctrl) {
            clearMouseCapture();
            return;
        }
        mouseCaptureTarget_ = createResolvedNodeCaptureTarget(ctrl, relativeTo);
    }

    void Scene::setMouseCaptureControl(AbstractControl* ctrl, AbstractControl* relativeTo)
    {
        if (!ctrl) {
            clearMouseCapture();
            return;
        }
        mouseCaptureTarget_ = createControlCaptureTarget(ctrl, relativeTo);
    }

    void Scene::clearMouseCapture()
    {
        mouseCaptureTarget_.reset();
    }

    ResolvedNode* Scene::getMouseCapture() const
    {
        return mouseCaptureTarget_ ? mouseCaptureTarget_->capturedNode() : nullptr;
    }

    void Scene::notifyNodeRemoved(ResolvedNode* node) {
        if (!node) return;

        if (auto ctrl = node->getControl().lock()) {
            pressedControls_.erase(ctrl.get());
        }
        
        // 1. 清理 UIEventSystem 状态
        uiEventSystem_.notifyNodeRemoved(node);
        
        // 2. 清理全局单例 UpdateScheduler 状态
        UpdateScheduler::getInstance().notifyNodeRemoved(node);
        
        // 3. 清理 Scene 自身持有的悬浮/捕获等引用
        if (mouseCaptureTarget_ && mouseCaptureTarget_->refersToNode(node)) {
            clearMouseCapture();
        }
        
        debugHitPath_.erase(std::remove(debugHitPath_.begin(), debugHitPath_.end(), node), debugHitPath_.end());
    }

} // namespace ldt
