#include "astbuild_engine.h"
#include "document_runtime.h"
#include "style_engine.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_node.h"
void ASTBuildEngine::process(std::shared_ptr<ASTNode> root, ldt::DisplayList& /*outList*/, float /*viewportW*/, float /*viewportH*/) {
    if (!root) return;
    if (!getContext()) return;
    ldt::ResolvedTree* rt = getContext()->getResolvedTree();
    if (!rt) return;

    // Use EngineContext so ResolvedTree can access engines internally
    DocumentRuntime* ctx = getContext();
    if (!lastAST_ || rt->getRoot() == nullptr) {
        rt->buildFromAST(root, ctx);
        lastAST_ = root;
    } else if (lastAST_ != root) {
        rt->updateFromAST(root, ctx);
        lastAST_ = root;
    }
}

void ASTBuildEngine::onASTUpdated(std::shared_ptr<ASTNode> root) {
    if (!root || !getContext()) return;
    ldt::ResolvedTree* rt = getContext()->getResolvedTree();
    if (rt) {
        rt->updateFromAST(root, getContext());
        lastAST_ = root;
    }
}
