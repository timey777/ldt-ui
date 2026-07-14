#include "hit_test_helper.h"
#include "engine/core/resolved_node.h"
#include <engine/core/resolved_tree_view.h>
namespace ldt {

ResolvedNode* HitTestHelper::findDeepestNodeAt(ResolvedNode* node, float vx, float vy) {
    if (!node) return nullptr;
    if (node->layoutRules.displayEnum == ldt::FormattingContext::None) return nullptr;
    if (!node->finalStyle.visible) return nullptr;

    // 0. 视觉坐标空间转换：传入的 vx/vy 是相对于父节点坐标空间的。
    // 如果此节点有 visualOffset，则需要将检测点投射到该节点的静态坐标空间中。
    float vOffX = 0, vOffY = 0;
    if (auto ctrl = node->getControl().lock()) {
        vOffX = ctrl->getVisualOffsetX();
        vOffY = ctrl->getVisualOffsetY();
    }
    float lx = vx - vOffX;
    float ly = vy - vOffY;

    const auto& l = node->layout;
    const auto paddingAbsBox = l.getAbsolutePaddingBox();

    // 1. 计算当前节点的绝对内容可视区域 (基于 lx/ly)
    // borderBox 是绝对坐标，paddingBox 是相对于 borderBox 的偏移
    float clipAbsX = paddingAbsBox.x;
    float clipAbsY = paddingAbsBox.y;

    // 2. 检查点是否在当前节点的可视内容区域内
    // viewport=0 表示 overflow=visible：没有视口裁剪，内容区为 paddingBox 全尺寸
    float contentW = (l.viewportWidth > 0.0f) ? l.viewportWidth : paddingAbsBox.width;
    float contentH = (l.viewportHeight > 0.0f) ? l.viewportHeight : paddingAbsBox.height;
    bool insideContent = (lx >= clipAbsX && lx <= clipAbsX + contentW &&
                          ly >= clipAbsY && ly <= clipAbsY + contentH);

    if (!insideContent) {
        // 如果不在内容区，检查是否在 BorderBox 范围内（命中边框或内边距）
        if (lx >= l.getBorderBox().x && lx <= l.getBorderBox().x + l.getBorderBox().width &&
            ly >= l.getBorderBox().y && ly <= l.getBorderBox().y + l.getBorderBox().height) {
            return node;
        }
        return nullptr;
    }

    // 获取当前节点的滚动偏移
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    if (auto ctrl = node->getControl().lock()) {
        scrollX = ctrl->getScrollX();
        scrollY = ctrl->getScrollY();
    }

    // 转换到内容坐标系 (针对子节点)
    float childHitX = lx + scrollX;
    float childHitY = ly + scrollY;

    // 3. 检查子节点（从上往下，z-order）
    for (auto it = node->getChildren().rbegin(); it != node->getChildren().rend(); ++it) {
        ResolvedNode* child = *it;
        if (!child) continue;
        if (child->layoutRules.displayEnum == ldt::FormattingContext::None) continue;
        if (!child->finalStyle.visible) continue;

        const auto& cb = child->layout.getBorderBox();

        // 使用加上了滚动偏移的坐标与子节点的绝对坐标比较
        if (childHitX >= cb.x && childHitX <= cb.x + cb.width &&
            childHitY >= cb.y && childHitY <= cb.y + cb.height) {
            
            // 递归查找，传递修正后的坐标
            if (auto deeper = findDeepestNodeAt(child, childHitX, childHitY)) {
                return deeper;
            }
            return child;
        }
    }

    // 4. 没有子节点命中，返回当前节点
    return node;
}

HitTestHelper::HitTestResult HitTestHelper::performHitTest(
    ResolvedTreeView* resolvedTree,
    LogicalPointDp point) {
    
    HitTestResult result;

    // 优先使用ResolvedTree进行命中测试
    if (resolvedTree && resolvedTree->getRoot()) {
        result.hitNode = findDeepestNodeAt(resolvedTree->getRoot(), point.x.value, point.y.value);
    }

    // 构建控件路径（仅基于 ResolvedTree）
    if (result.hitNode) {
        // 从 hitNode 向上遍历构建路径（保存 ResolvedNode*）
        ResolvedNode* curr = result.hitNode;
        while (curr) {
            result.path.insert(result.path.begin(), curr);
            curr = curr->parent;
        }
    }

    return result;
}

std::vector<HitTestHelper::TransformedCoord> HitTestHelper::calculatePathCoords(
    const std::vector<ResolvedNode*>& path,
    LogicalPointDp point) {

    std::vector<TransformedCoord> coords;
    coords.reserve(path.size());

    float currentX = point.x.value;
    float currentY = point.y.value;

    for (size_t i = 0; i < path.size(); ++i) {
        ResolvedNode* node = path[i];
        if (!node) {
            coords.push_back({ { 0.0f }, { 0.0f } });
            continue;
        }

        const auto& l = node->layout;
        auto ctrl = node->getControl().lock();

        // 视觉平移 ( visualOffset 是针对当前控件以及其子树整体的物理偏移 )
        if (ctrl) {
            currentX -= ctrl->getVisualOffsetX();
            currentY -= ctrl->getVisualOffsetY();
        }

        // 计算本地坐标 (相对于节点 borderBox 左上角，不包含滚动)
        coords.push_back({
            { currentX - l.getBorderBox().x },
            { currentY - l.getBorderBox().y }
        });

        // 累加当前节点的布局滚动偏移（供子节点使用）
        currentX += l.scroll.offsetX; 
        currentY += l.scroll.offsetY;
    }

    return coords;
}

NodeLocalPointDp HitTestHelper::transformToNodeLocalPoint(
    ResolvedNode* node,
    LogicalPointDp point) {
    if (!node) {
        return {};
    }

    std::vector<ResolvedNode*> path;
    auto* curr = node;
    while (curr) {
        path.push_back(curr);
        curr = curr->parent;
    }
    std::reverse(path.begin(), path.end());

    float currentX = point.x.value;
    float currentY = point.y.value;
    NodeLocalPointDp localPoint{};

    for (ResolvedNode* rn : path) {
        const auto& l = rn->layout;
        auto ctrl = rn->getControl().lock();

        if (ctrl) {
            currentX -= ctrl->getVisualOffsetX();
            currentY -= ctrl->getVisualOffsetY();
        }

        localPoint = {
            { currentX - l.getBorderBox().x },
            { currentY - l.getBorderBox().y }
        };

        currentX += l.scroll.offsetX;
        currentY += l.scroll.offsetY;
    }

    return localPoint;
}

bool HitTestHelper::transformToLocalSpace(ResolvedNode* node, float screenX, float screenY, float& outX, float& outY) {
    if (!node) return false;

    const NodeLocalPointDp localPoint = transformToNodeLocalPoint(node, { { screenX }, { screenY } });
    outX = localPoint.x.value;
    outY = localPoint.y.value;

    return true;
}
// bool HitTestHelper::transformToLocalSpace(ResolvedNode* node, float screenX, float screenY, float& outX, float& outY) {
//     if (!node) return false;

//     // 1. 向上构建路径（从目标到根）
//     std::vector<ResolvedNode*> path;
//     auto* curr = node;
//     while (curr) {
//         path.push_back(curr);
//         curr = curr->parent;
//     }
//     // 路径反转：根 -> 目标
//     std::reverse(path.begin(), path.end());

//     // 2. 模拟计算路径坐标（逻辑同 calculatePathCoords）
//     // 但此处我们只关心最后一个控件（目标）的计算结果

//     float accumulatedScrollX = 0.0f;
//     float accumulatedScrollY = 0.0f;
//     float lastLocalX = screenX;
//     float lastLocalY = screenY;

//     for (size_t i = 0; i < path.size(); ++i) {
//         auto* rn = path[i];
//         const auto& l = rn->layout;

//         float vOffX = 0, vOffY = 0;
//         if (auto ctrl = rn->getControl().lock()) {
//             vOffX = ctrl->getVisualOffsetX();
//             vOffY = ctrl->getVisualOffsetY();
//         }

//         // 还原 Layout 空间坐标
//         float layoutX = screenX + accumulatedScrollX;
//         float layoutY = screenY + accumulatedScrollY;

//         // 计算本地坐标
//         lastLocalX = layoutX - l.getBorderBox().x;
//         lastLocalY = layoutY - l.getBorderBox().y;

//         // 累加滚动供下一层使用
//         accumulatedScrollX += l.scroll.offsetX;
//         accumulatedScrollY += l.scroll.offsetY;
//     }

//     outX = lastLocalX;
//     outY = lastLocalY;
//     return true;
// }


} // namespace ldt
