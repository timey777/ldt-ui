#include "resolved_builder.h"

#include "engine/core/resolved_tree.h"
#include "engine/document_runtime.h"
#include "engine/layout_engine.h"
#include "engine/style_engine.h"

#include <atomic>
#include <optional>

namespace {

int nextLocalScopeId()
{
	static std::atomic<int> nextScopeId{ 1 };
	return nextScopeId.fetch_add(1, std::memory_order_relaxed);
}

StyleRuleScope toStyleRuleScope(ldt::ResolvedRuleScope ruleScope)
{
	return ruleScope == ldt::ResolvedRuleScope::Global ? StyleRuleScope::Global : StyleRuleScope::Local;
}

LayoutRuleScope toLayoutRuleScope(ldt::ResolvedRuleScope ruleScope)
{
	return ruleScope == ldt::ResolvedRuleScope::Global ? LayoutRuleScope::Global : LayoutRuleScope::Local;
}

void applyLocalScopeRecursively(const std::shared_ptr<ASTNode>& node, std::optional<int> localScopeId)
{
	if (!node)
	{
		return;
	}

	node->setLocalScopeId(localScopeId);
	for (const auto& child : node->children)
	{
		applyLocalScopeRecursively(child, localScopeId);
	}
}

std::optional<int> prepareLocalScope(const std::vector<std::shared_ptr<ASTNode>>& scopedRoots,
	ldt::ResolvedRuleScope ruleScope)
{
	if (ruleScope == ldt::ResolvedRuleScope::Global)
	{
		for (const auto& scopedRoot : scopedRoots)
		{
			applyLocalScopeRecursively(scopedRoot, std::nullopt);
		}
		return std::nullopt;
	}

	const int localScopeId = nextLocalScopeId();
	for (const auto& scopedRoot : scopedRoots)
	{
		applyLocalScopeRecursively(scopedRoot, localScopeId);
	}
	return localScopeId;
}

ldt::ResolvedNode* buildResolvedSubtreeWithRuleScope(std::shared_ptr<ASTNode> ast,
	const std::vector<std::shared_ptr<ASTNode>>& scopedRoots,
	DocumentRuntime* ctx,
	ldt::ResolvedRuleScope ruleScope)
{
	if (!ast || !ctx)
	{
		return nullptr;
	}

	const std::optional<int> localScopeId = prepareLocalScope(scopedRoots, ruleScope);

	if (auto* styleEngine = ctx->getStyleEngine())
	{
		styleEngine->setScopedRules(ast, toStyleRuleScope(ruleScope), localScopeId,
			ruleScope == ldt::ResolvedRuleScope::Local);
	}
	if (auto* layoutEngine = ctx->getLayoutEngine())
	{
		layoutEngine->setScopedRules(ast, toLayoutRuleScope(ruleScope), localScopeId,
			ruleScope == ldt::ResolvedRuleScope::Local);
	}

	ldt::ResolvedTree* rt = ctx->getResolvedTree();
	if (!rt)
	{
		return nullptr;
	}

	return rt->buildNodeRecursive(ast, ctx, nullptr);
}

} // namespace

namespace ldt {

ResolvedNode* ResolvedBuilder::BuildResolvedSubtreeFromAST(std::shared_ptr<ASTNode> ast,
	DocumentRuntime* ctx,
	ResolvedRuleScope ruleScope)
{
	return buildResolvedSubtreeWithRuleScope(ast, { ast }, ctx, ruleScope);
}

ResolvedNode* ResolvedBuilder::BuildResolvedSubtreeFromSnippet(const AstSnippet& snippet,
	DocumentRuntime* ctx,
	ResolvedRuleScope ruleScope)
{
	auto instantiatedSnippet = snippet.instantiate();
	if (!instantiatedSnippet)
	{
		return nullptr;
	}

	return buildResolvedSubtreeWithRuleScope(instantiatedSnippet.root(), instantiatedSnippet.contentRoots(), ctx, ruleScope);
}

} // namespace ldt