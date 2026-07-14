#include "resolved_tree_view.h"
#include "resolved_tree.h"
#include <misc/stable_id.h>
#include "misc/logger.h"
using namespace ldt;

ldt::ResolvedNode* ResolvedTreeView::getRoot() const
{
	return root_;
}
void ResolvedTreeView::attachSubtree(ResolvedNode* node, ResolvedNode* parent) {
    if (!node) return;
    // set parent and append to parent's children
    node->parent = parent;
    if (parent) {
        parent->addChildren(node);
    }
    else {
        // attaching as new root
        root_ = node;
    }

    // mark subtree dirty so layout/style will be computed
    node->markDirty(DirtyFlag::All);
}

void ResolvedTreeView::attachSubtreeFromOther(ResolvedTree& source, ResolvedNode* parent, bool shouldMarkDirty) {
    auto* movedRoot = source.getRoot();
    if (!movedRoot) return;

    if (parent) {
        parent->markDirty(DirtyFlag::Layout | DirtyFlag::ChildrenLayout);

        // The source tree's root is an auto-generated wrapper (e.g. "Root")
        // created by the parser/AST builder. Skip it and attach its children
        // directly — external callers shouldn't need to know about this wrapper.
        const auto& children = movedRoot->getChildren();
        for (auto* child : children) {
            if (!child) continue;
            if (parent->astNode && child->astNode) {
                try {
                    ldt::StableId::assignStableIdsUnderParent(parent->astNode, child->astNode);
                }
                catch (...) {
                    LDT_ERROR("ResolvedTreeView::attachSubtreeFromOther: StableId assignment threw");
                }
            }
            parent->addChildren(child);
            child->parent = parent;
        }
    }
    else {
        root_ = movedRoot;
    }
}