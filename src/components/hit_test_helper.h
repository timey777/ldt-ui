#pragma once

#include "abstract_control.h"
#include "container_control.h"
#include "engine/core/coordinate_types.h"
#include "engine/core/resolved_tree.h"
#include <memory>
#include <vector>

namespace ldt {

/**
 * @brief 命中测试辅助类 - 负责处理控件的命中检测逻辑
 */
class ResolvedTreeView;
class HitTestHelper {
public:
    using TransformedCoord = NodeLocalPointDp;

    /**
     * @brief 命中测试结果
     */
    struct HitTestResult {
        ResolvedNode* hitNode = nullptr;
        std::vector<ResolvedNode*> path;
    };


    /**
     * @brief 执行完整的命中测试（结合ResolvedTree和控件树）
     * @param controls 顶层控件列表
     * @param resolvedTree 解析树
     * @param x 全局x坐标
     * @param y 全局y坐标
     * @return 命中测试结果
     */
    static HitTestResult performHitTest(
        ResolvedTreeView* resolvedTree,
        LogicalPointDp point);

    /**
     * @brief 计算路径上每个控件的变换后坐标
     * @param path 控件路径
     * @param x 起始x坐标
     * @param y 起始y坐标
     * @return 变换后的坐标列表
     */
    static std::vector<TransformedCoord> calculatePathCoords(
        const std::vector<std::shared_ptr<AbstractControl>>& path,
        LogicalPointDp point);
    // Overload: calculate path coordinates from ResolvedNode* path
    static std::vector<TransformedCoord> calculatePathCoords(
        const std::vector<ResolvedNode*>& path,
        LogicalPointDp point);

    static NodeLocalPointDp transformToNodeLocalPoint(
        ResolvedNode* node,
        LogicalPointDp point);

    /**
     * @brief 计算单个控件下的本地坐标（从屏幕坐标转换）
     * 
     * @param node 目标节点
     * @param screenX 屏幕绝对坐标X
     * @param screenY 屏幕绝对坐标Y
     * @param outX 输出本地X
     * @param outY 输出本地Y
     * @return bool 成功返回 true
     */
    static bool transformToLocalSpace(ResolvedNode* node, float screenX, float screenY, float& outX, float& outY);

private:
    /**
     * @brief 在ResolvedTree中查找指定坐标最深的节点
     * @param node 起始节点
     * @param vx viewport-local x坐标
     * @param vy viewport-local y坐标
     * @return 命中的最深节点，未命中返回nullptr
     */
    static ResolvedNode* findDeepestNodeAt(ResolvedNode* node, float vx, float vy);

};

} // namespace ldt
