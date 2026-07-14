#pragma once

#include <string>
#include <memory>
#include "engine/core/parse.h"

namespace ldt {
    struct StableId {
        // 生成基于 path 的稳定字符串 id，例如 "sid_..."
        static std::string stableIdForPath(const std::string& path);

        // 递归为 AST 节点分配 uid 属性（会覆盖已有的 uid）
        static void assignStableIdsRecursive(const std::shared_ptr<ASTNode>& node, const std::string& parentPath = "");
        // 返回用于构造路径的节点段（type + 可选 key 属性），与之前 getNodeSegment 语义一致
        static std::string getNodeSegment(const std::shared_ptr<ASTNode>& node);
        // 为 node 的子树基于 parentAst 分配 uid（会考虑 parent 下已有同名段以添加索引）
        static void assignStableIdsUnderParent(const std::shared_ptr<ASTNode>& parentAst, const std::shared_ptr<ASTNode>& node);
    };
}
