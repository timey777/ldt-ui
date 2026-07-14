#pragma once

#include "ast_snippet.h"
#include "ast_node.h"
#include "resolved_rule_scope.h"
#include "resolved_node.h"
#include "ldt_export.h"
#include <memory>

class DocumentRuntime;

namespace ldt {

class LDT_API ResolvedBuilder {
public:
	static ResolvedNode* BuildResolvedSubtreeFromAST(std::shared_ptr<ASTNode> ast,
		DocumentRuntime* ctx = nullptr,
		ResolvedRuleScope ruleScope = ResolvedRuleScope::Local);
	static ResolvedNode* BuildResolvedSubtreeFromSnippet(const AstSnippet& snippet,
		DocumentRuntime* ctx = nullptr,
		ResolvedRuleScope ruleScope = ResolvedRuleScope::Local);
};

} // namespace ldt