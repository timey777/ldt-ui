#include "resolved_node_query.h"

#include <functional>
#include "resolved_tree_view.h"

namespace ldt {

ResolvedNode* ResolvedNodeQuery::FindById(ResolvedTreeView* tree, const std::string& id)
{
	if (!tree)
		return nullptr;
	auto root = tree->getRoot();
	if (!root)
		return nullptr;
	if (id.empty())
		return root;

	std::function<ResolvedNode*(ResolvedNode*)> dfs;
	dfs = [&](ResolvedNode* node) -> ResolvedNode* {
		if (!node)
			return nullptr;
		try
		{
			if (node->astNode && node->astNode->hasAttribute("id"))
			{
				auto attr = node->astNode->getAttribute("id");
				if (attr && attr->isString() && attr->as<std::string>() == id)
					return node;
			}
		}
		catch (...)
		{
		}

		for (const auto& child : node->getChildren())
		{
			if (!child)
				continue;
			if (auto* found = dfs(child))
				return found;
		}
		return nullptr;
	};

	return dfs(root);
}

} // namespace ldt