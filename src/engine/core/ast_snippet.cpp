#include "ast_snippet.h"

#include <utility>

namespace {

std::shared_ptr<ASTNode> cloneAstNodeTree(const std::shared_ptr<ASTNode>& node)
{
	if (!node)
	{
		return nullptr;
	}

	auto clone = std::make_shared<ASTNode>(node->type);
	clone->attrs = node->attrs;
	clone->localScopeId = node->localScopeId;
	clone->classList = node->classList;
	clone->childTypeCounters = node->childTypeCounters;

	clone->children.reserve(node->children.size());
	for (const auto& child : node->children)
	{
		auto clonedChild = cloneAstNodeTree(child);
		if (clonedChild)
		{
			clonedChild->parent = clone;
		}
		clone->children.push_back(std::move(clonedChild));
	}

	for (const auto& [key, metaNodes] : node->meta)
	{
		auto& clonedMetaNodes = clone->meta[key];
		clonedMetaNodes.reserve(metaNodes.size());
		for (const auto& metaNode : metaNodes)
		{
			clonedMetaNodes.push_back(cloneAstNodeTree(metaNode));
		}
	}

	return clone;
}

} // namespace

namespace ldt {

AstSnippet AstSnippet::FromParsedRoot(std::shared_ptr<ASTNode> root)
{
	return AstSnippet(std::move(root));
}

std::vector<std::shared_ptr<ASTNode>> AstSnippet::contentRoots() const
{
	if (!root_)
	{
		return {};
	}

	return root_->children;
}

const std::vector<std::shared_ptr<ASTNode>>* AstSnippet::getMeta(const std::string& key) const
{
	if (!root_)
	{
		return nullptr;
	}

	return root_->getMeta(key);
}

bool AstSnippet::hasMeta(const std::string& key) const
{
	return root_ && root_->hasMeta(key);
}

bool AstSnippet::hasContent() const
{
	return root_ && !root_->children.empty();
}

size_t AstSnippet::contentCount() const
{
	return root_ ? root_->children.size() : 0;
}

AstSnippet AstSnippet::instantiate() const
{
	return AstSnippet(cloneAstNodeTree(root_));
}

} // namespace ldt