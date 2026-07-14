#pragma once

#include "resolved_node.h"
#include "ldt_export.h"
#include <string>

namespace ldt {
class ResolvedTreeView;
class LDT_API ResolvedNodeQuery {
public:
	static ResolvedNode* FindById(ResolvedTreeView* tree, const std::string& id);
};

} // namespace ldt