#include "event_dispatcher.h"
#include "components/abstract_control.h"
#include "engine/ui_event_system.h"
#include "components/hit_test_helper.h"
#include "engine/core/resolved_node.h"
namespace ldt {

bool EventDispatcher::dispatchMouseButton(
    const std::vector<ResolvedNode*>& path,
    LogicalPointDp point,
    int button, int action,
    UIEventSystem* uiEventSystem,
    std::unordered_set<AbstractControl*>* pressedControls) {
    if (path.empty()) return false;

    // 计算路径上每个控件的坐标（基于 ResolvedNode 路径）
    auto pathCoords = HitTestHelper::calculatePathCoords(path, point);

    // 1. 优先处理 overlay（如滚动条）
    if (dispatchOverlayMouseButton(path, pathCoords, button, action)) {
        return true;
    }

    // 2. 从最深层开始向上冒泡分发
    bool handled = false;
    for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
        ResolvedNode* node = path[i];
        const auto& coord = pathCoords[i];
        if (!node) continue;
        auto ctrl = node->getControl().lock();
        if (!ctrl || !ctrl->isVisible()) continue;

        // Disabled controls block interactions but don't handle them
        if (!ctrl->isEnabled()) {
            handled = true;
            break;
        }

        handled = ctrl->onLocalMouseButton(toControlLocalPoint(coord), button, action);
        if (handled) break;
    }

    // 3. 分发 UIEventSystem 高层事件
    if (uiEventSystem) {
        // 分发 MouseDown/MouseUp
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            ResolvedNode* node = *it;
            if (!node) continue;
            auto ctrl = node->getControl().lock();
            if (!ctrl || !ctrl->isVisible() || !ctrl->isEnabled()) continue;

            UIEventSystem::UIEvent e;
            e.type = (action == 1) ? UIEventSystem::UIEventType::MouseDown
                                   : UIEventSystem::UIEventType::MouseUp;
            e.target = ctrl.get();
            uiEventSystem->dispatch(e);

            if (e.stopPropagation) break;

            // 记录按下的控件
            if (action == 1 && pressedControls) {
                pressedControls->insert(ctrl.get());
            }
        }

        // 在鼠标释放时分发 Click 事件
        if (action == 0 && pressedControls) {
            for (auto it = path.rbegin(); it != path.rend(); ++it) {
                ResolvedNode* node = *it;
                if (!node) continue;
                auto ctrl = node->getControl().lock();
                if (!ctrl || !ctrl->isVisible() || !ctrl->isEnabled()) continue;

                if (pressedControls->find(ctrl.get()) != pressedControls->end()) {
                    UIEventSystem::UIEvent e;
                    e.type = UIEventSystem::UIEventType::Click;
                    e.target = ctrl.get();
                    uiEventSystem->dispatch(e);

                    if (e.stopPropagation) break;
                }
            }
        }
    }

    return handled;
}

void EventDispatcher::dispatchMouseMove(
    const std::vector<ResolvedNode*>& path,
    LogicalPointDp point) {

    if (path.empty()) return;

    auto pathCoords = HitTestHelper::calculatePathCoords(path, point);

    // 1. 处理 overlay（从外到内）
    dispatchOverlayMouseMove(path, pathCoords);

    // 2. 从最深层开始向上冒泡分发（从内到外）
    for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
        ResolvedNode* node = path[i];
        const auto& coord = pathCoords[i];
        if (!node) continue;
        auto ctrl = node->getControl().lock();
        if (!ctrl || !ctrl->isVisible()) continue;

        if (!ctrl->isEnabled()) break;

        if (ctrl->onLocalMouseMove(toControlLocalPoint(coord))) {
            break; // 事件被处理，停止冒泡
        }
    }
}

bool EventDispatcher::dispatchScroll(
    std::shared_ptr<AbstractControl> ctrl,
    LogicalPointDp point,
    LogicalScrollDeltaDp delta) {
    
    // x, y 始终保持为绝对屏幕坐标
    // hitTest 现已改为接受绝对坐标
    if (!ctrl || !ctrl->isVisible() || !ctrl->hitTestPoint(point)) {
        return false;
    }

    // 先尝试子控件（从上往下）
    if (auto container = std::dynamic_pointer_cast<ContainerControl>(ctrl)) {
        const auto& children = container->getChild();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            // 直接传递绝对坐标，无需转换
            if (dispatchScroll(*it, point, delta)) {
                return true;
            }
        }
    }

    // 子控件未处理，尝试自身
    return ctrl->onLocalScroll({ delta.dx, delta.dy });
}

bool EventDispatcher::dispatchOverlayMouseButton(
    const std::vector<ResolvedNode*>& path,
    const std::vector<NodeLocalPointDp>& pathCoords,
    int button, int action) {

    // 从外向内遍历（让最外层的 overlay 优先处理）
    for (size_t i = 0; i < path.size(); ++i) {
        ResolvedNode* node = path[i];
        const auto& coord = pathCoords[i];
        if (!node) continue;
        auto ctrl = node->getControl().lock();
        if (!ctrl || !ctrl->isVisible() || !ctrl->isEnabled()) continue;

        if (auto container = std::dynamic_pointer_cast<ContainerControl>(ctrl)) {
            if (container->onOverlayMouseButton(toParentLocalPoint(coord), button, action)) {
                return true;
            }
        }
    }

    return false;
}

void EventDispatcher::dispatchOverlayMouseMove(
    const std::vector<ResolvedNode*>& path,
    const std::vector<NodeLocalPointDp>& pathCoords) {

    // 从外向内遍历
    for (size_t i = 0; i < path.size(); ++i) {
        ResolvedNode* node = path[i];
        const auto& coord = pathCoords[i];
        if (!node) continue;
        auto ctrl = node->getControl().lock();
        if (!ctrl || !ctrl->isVisible() || !ctrl->isEnabled()) continue;

        if (auto container = std::dynamic_pointer_cast<ContainerControl>(ctrl)) {
            // 手动检查滚动条
            if (auto vScrollbar = container->getVScrollbar()) {
                if (vScrollbar->isVisible()) {
                    vScrollbar->onParentLocalMouseMove(toParentLocalPoint(coord));
                }
            }
            if (auto hScrollbar = container->getHScrollbar()) {
                if (hScrollbar->isVisible()) {
                    hScrollbar->onParentLocalMouseMove(toParentLocalPoint(coord));
                }
            }
        }
    }
}

} // namespace ldt
