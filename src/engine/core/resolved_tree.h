#pragma once

#include "parse.h"
#include "render/display_list.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <optional>
#include <array>
#include <string>
#include "node_flags.h"
#include "resolved_node.h"

class DocumentRuntime;

namespace ldt {
    class ResolvedNode;
    // 解析后树
    class ResolvedTree {
    public:
        ResolvedTree() = default;
        ~ResolvedTree() = default;

        // 从 AST 构建解析后树
        // 可选传入 DocumentRuntime*，如果提供则在创建节点时使用其中的 Style/Layout 引擎预编译样式规则并挂载到节点
        void buildFromAST(std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx = nullptr);

        // 增量更新：比较新旧 AST，只标记变化的节点为脏
        void updateFromAST(std::shared_ptr<ASTNode> newAst);
        // Overload: 提供 DocumentRuntime 以便在更新期间重新编译样式和布局规则
        void updateFromAST(std::shared_ptr<ASTNode> newAst, DocumentRuntime* ctx);

        // 获取根节点
        ldt::ResolvedNode* getRoot() const { return root_; }

        // 收集所有需要重新布局的节点
        void collectDirtyNodes(std::vector<ldt::ResolvedNode*>& outNodes, ldt::DirtyFlag filter);

        // 清空整个树
        void clear();

        //bool detachSubtree(ldt::ResolvedNode* node, bool shouldMarkParentDirty = true);
        // 将已有的 ResolvedNode 子树挂载到指定父节点下，并注册映射
        //void attachSubtree(ldt::ResolvedNode* node, ResolvedNode* parent);
        // 高效合并：将另一个 ResolvedTree 的根子树移动并附加到本树的 parent 下，
        // 同时合并并偷取对方的 ast->node 映射，避免对整个子树进行逐节点遍历注册。
        // 操作后 source.root_ 会被清空，source 的映射也会被清除。
        //void attachSubtreeFromOther(ResolvedTree& source, ldt::ResolvedNode* parent, bool shouldMarkDirty = true);

        // 查找对应 AST 节点的 ResolvedNode
        //ResolvedNode* findRenderNode(std::shared_ptr<ASTNode> astNode);
        // 递归构建（公开）：将 parent 参数放到最后，默认 nullptr；可传入 DocumentRuntime 以使用 Style/Layout 引擎
        ldt::ResolvedNode* buildNodeRecursive(std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx = nullptr, ldt::ResolvedNode* parent = nullptr);

    private:
        ldt::ResolvedNode* root_ = nullptr;
        std::vector<std::unique_ptr<ldt::ResolvedNode>> ownedNodes_;

        // 递归更新（可选接收 DocumentRuntime 用于重新编译规则）
        void updateNodeRecursive(ldt::ResolvedNode* renderNode, std::shared_ptr<ASTNode> newAst, DocumentRuntime* ctx = nullptr);
        ldt::ResolvedNode* createNode(std::shared_ptr<ASTNode> ast);
        void recycleNode(ldt::ResolvedNode* node);

        // 比较 AST 节点是否相同（结构和属性）
        bool isSameNode(std::shared_ptr<ASTNode> a, std::shared_ptr<ASTNode> b);

        // 递归收集脏节点
        void collectDirtyNodesRecursive(ldt::ResolvedNode* node, std::vector<ldt::ResolvedNode*>& outNodes, DirtyFlag filter);
    };

}