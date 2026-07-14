// update_scheduler.h
#pragma once

#include <vector>
#include <unordered_set>
#include "document_update_plan.h"

namespace ldt{
    class ResolvedNode;
}
#include "ldt_export.h"

class LDT_API UpdateScheduler {
public:
    static UpdateScheduler& getInstance();

    // --- 外部调用接口 ---
    
    // 1. 节点请求更新 (通常由 ResolvedNode 内部调用)
    void requestUpdate(ldt::ResolvedNode* node);

    // 移除节点时清空相关引用
    void notifyNodeRemoved(ldt::ResolvedNode* node);

    // 2. 请求纯重绘 (无节点逻辑)
    void requestPaint();

    // 3. 请求重排+重绘（样式更新）
    void requestRepaint();

    // 4. 请求 AST 重建+重绘
    void requestASTRepaint();

    // 消费本帧挂起的更新计划（分析 + 清空），返回给调用方自行执行。
    DocumentUpdatePlan consumePendingPlan();

private:
    UpdateScheduler() = default;
    ~UpdateScheduler() = default;
    UpdateScheduler(const UpdateScheduler&) = delete;
    UpdateScheduler& operator=(const UpdateScheduler&) = delete;

    enum class FrameAction {
        None = 0,
        PaintOnly,      // 仅重绘
        PartialUpdate,  // 局部更新（样式/简单布局）
        Repaint,        // 全量样式/布局同步
        FullLayout,     // 全量重排（一旦有一个节点触发了流式布局改变）
        ASTRepaint      // AST 更新引发的全量重建
    };

    FrameAction analyzeFrameAction();
    const std::vector<ldt::ResolvedNode*>& getDirtyNodes() const;
    void clear();

    std::unordered_set<ldt::ResolvedNode*> m_dirtyNodesSet;
    mutable std::vector<ldt::ResolvedNode*> m_dirtyNodesVec;
    bool m_hasPaintRequest = false;
    bool m_hasRepaintRequest = false;
    bool m_hasASTRepaintRequest = false;
};
