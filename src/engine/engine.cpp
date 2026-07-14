#include "engine.h"

// Default empty implementations for AbstractEngine moved from header
void AbstractEngine::initialize() {}
void AbstractEngine::shutdown() {}
void AbstractEngine::setViewport(float /*width*/, float /*height*/) {}
void AbstractEngine::onASTUpdated(std::shared_ptr<ASTNode> /*root*/) {}
void AbstractEngine::update(float /*dt*/) {}
void AbstractEngine::setContext(DocumentRuntime* ctx) { context_ = ctx; }
DocumentRuntime* AbstractEngine::getContext() const { return context_; }
