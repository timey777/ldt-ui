#include "resolved_mount_service.h"

#include "container_control.h"
#include "control_factory.h"
#include "engine/core/resolved_builder.h"
#include "engine/core/resolved_node_query.h"
#include "misc/stable_id.h"

namespace ldt {

std::shared_ptr<AbstractControl> MountResult::primary() const
{
	return controls.empty() ? nullptr : controls.front();
}

bool MountResult::empty() const
{
	return controls.empty();
}

MountResult::operator bool() const
{
	return success;
}

MountResult ResolvedMountService::MountResolvedSubtreeUnderId(ResolvedTreeView* rt, const std::string& parentId,
	ResolvedNode* subtree, DocumentRuntime* ctx, ControlFactory* factory)
{
	MountResult result;
	if (!rt || !subtree)
		return result;

	ControlFactory* effectiveFactory = factory ? factory : ControlFactory::getInstance();
	if (!effectiveFactory)
		return result;

	ResolvedNode* parent = ResolvedNodeQuery::FindById(rt, parentId);
	if (!parent)
		return result;

	auto pushNode = [&](ResolvedNode* node) -> void {
		if (!node)
			return;

		rt->attachSubtree(node, parent);

		if (parent->astNode && node->astNode)
		{
			try
			{
				parent->astNode->children.push_back(node->astNode);
				node->astNode->parent = parent->astNode;
			}
			catch (...)
			{
				LDT_ERROR("ResolvedMountService: failed to attach AST node");
			}

			try
			{
				StableId::assignStableIdsUnderParent(parent->astNode, node->astNode);
			}
			catch (...)
			{
				LDT_ERROR("ResolvedMountService: StableId assignment failed");
			}
		}

		auto ctrl = node->astNode ? effectiveFactory->CreateControlTreeFromAST(node) : nullptr;
		if (ctrl)
		{
			if (auto parentCtrl = parent->getControl().lock())
			{
				if (auto container = std::dynamic_pointer_cast<ContainerControl>(parentCtrl))
				{
					container->addChild(ctrl);
				}
			}
		}

		result.controls.push_back(ctrl);
	};

	if (subtree->getChildren().empty())
	{
		pushNode(subtree);
	}
	else
	{
		for (auto* child : subtree->getChildren())
		{
			pushNode(child);
		}
	}

	result.success = !result.controls.empty();
	return result;
}

MountResult ResolvedMountService::MountAstUnderId(ResolvedTreeView* rt, const std::string& parentId,
	std::shared_ptr<ASTNode> ast, DocumentRuntime* ctx, ResolvedRuleScope ruleScope, ControlFactory* factory)
{
	auto subtree = ResolvedBuilder::BuildResolvedSubtreeFromAST(ast, ctx, ruleScope);
	return MountResolvedSubtreeUnderId(rt, parentId, subtree, ctx, factory);
}

MountResult ResolvedMountService::MountSnippetUnderId(ResolvedTreeView* rt, const std::string& parentId,
	const AstSnippet& snippet, DocumentRuntime* ctx, ResolvedRuleScope ruleScope, ControlFactory* factory)
{
	auto subtree = ResolvedBuilder::BuildResolvedSubtreeFromSnippet(snippet, ctx, ruleScope);
	return MountResolvedSubtreeUnderId(rt, parentId, subtree, ctx, factory);
}

} // namespace ldt