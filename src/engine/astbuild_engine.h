#pragma once

#include "engine.h"
#include "core/parse.h"
#include <memory>

class ASTBuildEngine : public AbstractEngine {
public:
    ASTBuildEngine() = default;
    ~ASTBuildEngine() = default;

    void initialize() override {}
    void shutdown() override {}

    std::type_index type() const override {
        return typeid(ASTBuildEngine);
    }
    // Build or update the shared ResolvedTree in EngineContext
    void process(std::shared_ptr<ASTNode> root, ldt::DisplayList& outList, float viewportW, float viewportH) override;

    void onASTUpdated(std::shared_ptr<ASTNode> root) override;

    std::string name() const override { return "ASTBuildEngine"; }

private:
    std::shared_ptr<ASTNode> lastAST_;
};
