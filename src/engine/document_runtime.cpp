#include "document_runtime.h"
#include "style_engine.h"
#include "box_model_engine.h"
#include "layout_engine.h"
#include "astbuild_engine.h"
#include "update_scheduler.h"
#include "misc/ui_diagnostics.h"

namespace {
void run_engine_with_scratch(AbstractEngine* engine,
                             const std::shared_ptr<ASTNode>& ast,
                             float viewportW,
                             float viewportH) {
    ldt::DisplayList scratch;
    engine->process(ast, scratch, viewportW, viewportH);
}
}

void DocumentRuntime::initializeAll(std::vector<std::type_index> pipelineOrder) {
    serviceRegistry.order(pipelineOrder);
    serviceRegistry.forEach([](AbstractEngine* engine) {
        engine->initialize();
    });
}

void DocumentRuntime::shutdownAll() {
    serviceRegistry.forEach([](AbstractEngine* engine) {
        engine->shutdown();
    });
}

void DocumentRuntime::registerService(AbstractEngine* service) {
    service->setContext(this);
    serviceRegistry.registerService(service);
}

void DocumentRuntime::runPipeline(ldt::DisplayList& outList, float viewportW, float viewportH) {
    (void)outList;
    if (!m_ast) return;
    serviceRegistry.forEach([&](AbstractEngine* engine) {
        run_engine_with_scratch(engine, m_ast, viewportW, viewportH);
    });
}

void DocumentRuntime::runPipeline(const std::vector<std::type_index>& pipelineOrder, float viewportW, float viewportH) {
    if (!m_ast) return;
    serviceRegistry.forEach(pipelineOrder, [&](AbstractEngine* engine) {
        run_engine_with_scratch(engine, m_ast, viewportW, viewportH);
    });
}

void DocumentRuntime::runPipelineRenderTreeOnly(ldt::ViewportSizeDp viewportSize) {
    if (!m_ast) return;
    serviceRegistry.forEach([&](AbstractEngine* engine) {
        run_engine_with_scratch(engine, m_ast, viewportSize.width.value, viewportSize.height.value);
    });
}

void DocumentRuntime::runPipelineLayoutOnly(ldt::ViewportSizeDp viewportSize) {
    if (!m_ast) return;
    const std::vector<std::type_index> pipelineOrder = { typeid(BoxModelEngine) };
    serviceRegistry.forEach(pipelineOrder, [&](AbstractEngine* engine) {
        run_engine_with_scratch(engine, m_ast, viewportSize.width.value, viewportSize.height.value);
    });
}

LayoutEngine* DocumentRuntime::getLayoutEngine() const { return getService<LayoutEngine>(); }
StyleEngine* DocumentRuntime::getStyleEngine() const { return getService<StyleEngine>(); }
BoxModelEngine* DocumentRuntime::getBoxModelEngine() const { return getService<BoxModelEngine>(); }
ASTBuildEngine* DocumentRuntime::getASTBuildEngine() const { return getService<ASTBuildEngine>(); }

void DocumentRuntime::notifyASTUpdated(std::shared_ptr<ASTNode> root) {
    if (!root) return;
    setAST(root);
    UpdateScheduler::getInstance().requestASTRepaint();
}

void DocumentRuntime::setAST(std::shared_ptr<ASTNode> a) { m_ast = a; }
std::shared_ptr<ASTNode> DocumentRuntime::getAST() { return m_ast; }

ldt::ResolvedTree* DocumentRuntime::getResolvedTree() { return &resolvedTree; }
const ldt::ResolvedTree* DocumentRuntime::getResolvedTree() const { return &resolvedTree; }

void DocumentRuntime::setTextMeasurer(ITextMeasurer* m) { textMeasurer = m; }
ITextMeasurer* DocumentRuntime::getTextMeasurer() const { return textMeasurer; }


