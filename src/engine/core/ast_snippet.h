#pragma once

#include "ast_node.h"
#include "ldt_export.h"
#include <memory>
#include <string>
#include <vector>

namespace ldt {

class LDTParser;

class LDT_API AstSnippet {
public:
	AstSnippet() = default;
	static AstSnippet FromParsedRoot(std::shared_ptr<ASTNode> root);

	const std::shared_ptr<ASTNode>& root() const { return root_; }
	std::vector<std::shared_ptr<ASTNode>> contentRoots() const;
	const std::vector<std::shared_ptr<ASTNode>>* getMeta(const std::string& key) const;
	bool hasMeta(const std::string& key) const;
	bool hasContent() const;
	size_t contentCount() const;
	AstSnippet instantiate() const;
	bool empty() const { return !root_; }
	explicit operator bool() const { return root_ != nullptr; }

private:
	explicit AstSnippet(std::shared_ptr<ASTNode> root) : root_(std::move(root)) {}

	std::shared_ptr<ASTNode> root_;

	friend class LDTParser;
};

} // namespace ldt