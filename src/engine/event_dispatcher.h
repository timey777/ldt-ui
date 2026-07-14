#pragma once

#include <unordered_set>
#include <memory>
#include <vector>
#include "engine/core/resolved_tree.h"
#include "engine/core/coordinate_types.h"
#include "components/hit_test_helper.h"
#include "components/abstract_control.h"
namespace ldt {
class UIEventSystem;

/**
 * @brief 事件分发器 - 负责将鼠标/键盘事件分发到控件树
 */
class EventDispatcher {
public:
    /**
     * @brief 分发鼠标按钮事件到路径上的控件
     * @param path 控件路径（从根到叶）
     * @param x 全局x坐标
     * @param y 全局y坐标
     * @param button 鼠标按钮
     * @param action 动作（按下/释放）
     * @param uiEventSystem UIEventSystem指针（用于分发高层事件）
     * @param pressedControls 按下控件集合（用于click检测）
     * @return 是否被处理
     */
    static bool dispatchMouseButton(
        const std::vector<ResolvedNode*>& path,
        LogicalPointDp point,
        int button, int action,
        UIEventSystem* uiEventSystem,
        std::unordered_set<AbstractControl*>* pressedControls);

    /**
     * @brief 分发鼠标移动事件到路径上的控件
     * @param path 控件路径（从根到叶）
     * @param x 全局x坐标
     * @param y 全局y坐标
     */
    static void dispatchMouseMove(
        const std::vector<ResolvedNode*>& path,
        LogicalPointDp point);

    /**
     * @brief 分发滚轮事件
     * @param ctrl 起始控件
     * @param x 全局x坐标
     * @param y 全局y坐标
     * @param dx 水平滚动增量
     * @param dy 垂直滚动增量
     * @return 是否被处理
     */
    static bool dispatchScroll(
        std::shared_ptr<AbstractControl> ctrl,
        LogicalPointDp point,
        LogicalScrollDeltaDp delta);

private:
    /**
     * @brief 处理overlay（如滚动条）的鼠标按钮事件
     */
    static bool dispatchOverlayMouseButton(
        const std::vector<ResolvedNode*>& path,
        const std::vector<NodeLocalPointDp>& pathCoords,
        int button, int action);

    /**
     * @brief 处理overlay的鼠标移动事件
     */
    static void dispatchOverlayMouseMove(
        const std::vector<ResolvedNode*>& path,
        const std::vector<NodeLocalPointDp>& pathCoords);
};

} // namespace ldt
