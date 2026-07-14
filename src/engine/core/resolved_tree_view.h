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
    class ResolvedTreeView {
    private:
        ldt::ResolvedNode* root_ = nullptr;
    public:
        ResolvedTreeView() = default;
        ~ResolvedTreeView() = default;

        // 获取根节点
        ldt::ResolvedNode* getRoot() const;
        void attachSubtree(ResolvedNode* node, ResolvedNode* parent);
        void attachSubtreeFromOther(ResolvedTree& source, ldt::ResolvedNode* parent, bool shouldMarkDirty = true);
    };

}