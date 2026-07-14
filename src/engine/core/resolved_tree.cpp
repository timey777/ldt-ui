#include "resolved_tree.h"
#include "engine/document_runtime.h"
#include "engine/style_engine.h"
#include "engine/layout_engine.h"
#include "misc/stable_id.h"
#include "components/abstract_control.h"
#include <components/control_factory.h>
#include <algorithm>
#include "engine/core/ast_node.h"
#include "resolved_node.h"
#include "engine/update_scheduler.h"
using namespace ldt;

// ============ ResolvedTree 实现 ============

void ResolvedTree::buildFromAST(std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx) {
    clear();
    if (!ast) return;

    // Ensure AST nodes have stable internal ids before building the resolved tree
    ldt::StableId::assignStableIdsRecursive(ast, "");

    root_ = buildNodeRecursive(ast, ctx, nullptr);
}

ResolvedNode* ResolvedTree::createNode(std::shared_ptr<ASTNode> ast) {
    std::unique_ptr<ResolvedNode> node(new ResolvedNode(this));
    node->parent = nullptr;
    node->astNode = ast;

    auto* rawNode = node.get();
    ownedNodes_.push_back(std::move(node));
    return rawNode;
}

ResolvedNode* ResolvedTree::buildNodeRecursive(std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx, ResolvedNode* parent) {
    if (!ast) return nullptr;
    
    auto* renderNode = createNode(ast);
    renderNode->parent = parent;
    renderNode->markDirty(DirtyFlag::All);
    renderNode->recompileStyleAndLayout(ctx);
    // 递归构建子节点
    for (auto& childAst : ast->children) {
        if (auto* childRender = buildNodeRecursive(childAst, ctx, renderNode)) {
            renderNode->addChildren(childRender);
        }
    }
    
    return renderNode;
}

void ResolvedTree::updateFromAST(std::shared_ptr<ASTNode> newAst) {
    if (!root_ || !newAst) {
        // 如果没有旧树或新 AST 为空，直接重建
        buildFromAST(newAst, nullptr);
        return;
    }
    
    // 不提供引擎指针的旧版本：不进行编译规则更新
    updateNodeRecursive(root_, newAst, nullptr);
}

void ResolvedTree::updateFromAST(std::shared_ptr<ASTNode> newAst, DocumentRuntime* ctx) {
    if (!root_ || !newAst) {
        buildFromAST(newAst, ctx);
        return;
    }

    updateNodeRecursive(root_, newAst, ctx);
    // TODO 待优化，不管有没有uid，临时全部都设置uid
    ldt::StableId::assignStableIdsRecursive(newAst, "");
}
void ResolvedTree::updateNodeRecursive(ResolvedNode* renderNode, std::shared_ptr<ASTNode> newAst, DocumentRuntime* ctx) {
    if (!renderNode || !newAst) return;

    bool structureChanged = false;

    // 检查节点本身是否改变（类型/uid）
    if (!isSameNode(renderNode->astNode, newAst)) {
        // 节点属性或类型改变，标记为脏
        renderNode->markDirty(DirtyFlag::All);
        renderNode->astNode = newAst;
        structureChanged = true;
    }

    renderNode->astNode = newAst;

    // 若节点本体没有结构变化，但属性/样式可能更新，使用 DocumentRuntime 提供的引擎重新编译样式/布局
    if (!structureChanged && ctx) {
        auto se = ctx->getStyleEngine();
        auto le = ctx->getLayoutEngine();
        if (se) {
            try {
                auto compiledStyle = se->compile(newAst);
                renderNode->setCompiledStyleRules(compiledStyle);
            } catch(...) {}
        }
        if (le) {
            try {
                auto compiledLayout = le->compile(newAst);
                renderNode->setCompiledLayoutRules(compiledLayout);
            } catch(...) {}
        }
    }

    // 比较子节点
    size_t oldCount = renderNode->getChildren().size();
    size_t newCount = newAst->children.size();

    if (oldCount != newCount) {
        structureChanged = true;
    }

    if (structureChanged) {
        // 结构变化时，重建子树；传入引擎以便构建时立即编译规则
        renderNode->clear();
        for (auto& childAst : newAst->children) {
            if (auto* childRender = buildNodeRecursive(childAst, ctx, renderNode)) {
                renderNode->addChildren(childRender);
            }
        }
        renderNode->markDirty(DirtyFlag::All);
    } else {
        // 递归更新子节点，传入引擎以便子节点也能重新编译
        for (size_t i = 0; i < oldCount; ++i) {
            updateNodeRecursive(renderNode->getChildren()[i], newAst->children[i], ctx);
        }
    }
}

bool ResolvedTree::isSameNode(std::shared_ptr<ASTNode> a, std::shared_ptr<ASTNode> b) {
    if (!a || !b) return false;

    // 先比较类型，避免不同类型但 uid 意外相同的极端情况
    if (a->type != b->type) return false;

    // 优先使用内部稳定 uid 判断是否同一逻辑节点
    auto uidA = a->getUid();
    auto uidB = b->getUid();
    if (uidA && uidB && uidA->isString() && uidB->isString()) {
        return uidA->as<std::string>() == uidB->as<std::string>();
    }

    return false;
}

void ResolvedTree::collectDirtyNodes(std::vector<ResolvedNode*>& outNodes, DirtyFlag filter) {
    if (!root_) return;
    collectDirtyNodesRecursive(root_, outNodes, filter);
}

void ResolvedTree::collectDirtyNodesRecursive(ResolvedNode* node, std::vector<ResolvedNode*>& outNodes, DirtyFlag filter) {
    if (!node) return;
    
    if (node->isDirty(filter)) {
        outNodes.push_back(node);
    }
    
    for (auto* child : node->getChildren()) {
        collectDirtyNodesRecursive(child, outNodes, filter);
    }
}


void ResolvedTree::clear() {
    for (const auto& ownedNode : ownedNodes_) {
        if (ownedNode) {
            recycleNode(ownedNode.get());
        }
    }
    root_ = nullptr;
    ownedNodes_.clear();
    // 清空 control_factory 中为本树维护的 uid 映射
    try {
        ControlFactory::getInstance()->ClearUidMap();
    } catch(...) {}
}

//bool ResolvedTree::detachSubtree(ResolvedNode* node, bool shouldMarkParentDirty) {
//    if (!node) {
//        return false;
//    }
//
//    if (root_ == node) {
//        root_ = nullptr;
//        //destroySubtree(node);
//        return true;
//    }
//
//    auto* parent = node->parent;
//    if (!parent) {
//        return false;
//    }
//
//    auto detached = parent->extractChild(node);
//    if (!detached) {
//        return false;
//    }
//
//    if (shouldMarkParentDirty) {
//        parent->markDirty(DirtyFlag::Layout | DirtyFlag::ChildrenLayout);
//    }
//
//    //destroySubtree(detached);
//    return true;
//}

void ResolvedTree::recycleNode(ResolvedNode* node) {
    if (!node) {
        return;
    }

    // 统一节点回收入口。后续若需要在回收时发通知，直接放在这里。
    UpdateScheduler::getInstance().notifyNodeRemoved(node);
}

//void ResolvedTree::attachSubtreeFromOther(ResolvedTree& source, ResolvedNode* parent, bool shouldMarkDirty) {
//    // If source is this tree, skip (avoid attaching tree into itself)
//    if (&source == this) return;
//
//    if (!source.root_) return;
//
//    auto* movedRoot = source.root_;
//    if (!movedRoot) return;
//
//    // Transfer all node ownership from source to this tree. Nodes themselves stay at
//    // stable addresses because only the unique_ptr owners move.
//    ownedNodes_.reserve(ownedNodes_.size() + source.ownedNodes_.size());
//    for (auto& node : source.ownedNodes_) {
//        ownedNodes_.push_back(std::move(node));
//    }
//    source.ownedNodes_.clear();
//
//    // for the incoming AST subtree so ids are relative to the new parent.
//    if (parent) {
//        parent->markDirty(DirtyFlag::Layout|DirtyFlag::ChildrenLayout);
//        if (parent->astNode && movedRoot->astNode) {
//            try {
//                ldt::StableId::assignStableIdsUnderParent(parent->astNode, movedRoot->astNode);
//            } catch (...) {}
//        }
//        parent->addChildren(movedRoot);
//        movedRoot->parent = parent;
//    } else {
//        root_ = movedRoot;
//    }
//
//    source.root_ = nullptr;
//
//    // mark subtree dirty so layout/style will be computed on transferred nodes
//    if (shouldMarkDirty) {
//        movedRoot->markDirty(DirtyFlag::All);
//    }
//}
