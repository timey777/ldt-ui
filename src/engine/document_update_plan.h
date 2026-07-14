#pragma once

#include <vector>

namespace ldt {
class ResolvedNode;
}

// 文档更新计划：由 UpdateScheduler 产出，ViewCoordinator 消费。
// 不包含 viewport/scene 等视图状态，仅描述本次应执行的引擎工作类别。

enum class DocumentUpdateKind {
    None = 0,
    PaintOnly,      // 仅重绘，不重新计算样式/布局
    PartialUpdate,  // 局部样式/布局同步（仅指定脏节点）
    Repaint,        // 全量样式/布局同步
    FullLayout,     // 全量重排（有节点触发了流式布局改变）
    ASTRepaint,     // AST 结构更新，全量重建
};

struct DocumentUpdatePlan {
    DocumentUpdateKind kind = DocumentUpdateKind::None;
    std::vector<ldt::ResolvedNode*> dirtyNodes;

    bool hasWork() const { return kind != DocumentUpdateKind::None; }
};
