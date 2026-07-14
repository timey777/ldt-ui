#pragma once

#include "engine.h"
#include "render/display_list.h"
#include "core/resolved_tree.h"
#include <map>
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "ldt_export.h"

namespace ldt {
    struct CompiledLayoutRules;
}
// 布局规则（类似 StyleRule）
struct LayoutRule {
    std::string selector; // 选择器，如 "panel", ".container"
    std::map<std::string, Attribute> properties; // 布局属性（display, position, flex* 等）
};

struct LayoutRuleBucket {
    std::vector<LayoutRule> rules;

    void clear() {
        rules.clear();
    }
};

enum class LayoutRuleScope {
    Default,
    Global,
    Local
};

class LDT_API LayoutEngine : public AbstractEngine {
public:
    LayoutEngine();
    ~LayoutEngine();
    
    void initialize() override;
    void shutdown() override;
    
    std::type_index type() const override;
    void process(std::shared_ptr<ASTNode> root, ldt::DisplayList& outList, 
                 float viewportW, float viewportH) override;
    
    std::string name() const override;
    
    // 热重载支持：增量更新
    void onASTUpdated(std::shared_ptr<ASTNode> root) override;
    
    // 获取匹配节点的布局属性（返回 Attribute 指针）
    const Attribute* getLayoutProperty(std::shared_ptr<ASTNode> node, const std::string& property) const;
    
    // 获取布局属性的字符串值（便捷方法）
    std::string getLayoutPropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                       const std::string& defaultValue = "") const;
    
    // 检查节点是否有布局属性
    bool hasLayoutProperty(std::shared_ptr<ASTNode> node, const std::string& property) const;
    
    // 清空所有布局规则
    void clearLayoutRules();

    void setScopedRules(std::shared_ptr<ASTNode> root, LayoutRuleScope scope,
                        std::optional<int> ownerId = std::nullopt, bool replace = true);
    void clearScopedRules(LayoutRuleScope scope, std::optional<int> ownerId = std::nullopt);
    
    // 加载默认布局规则
    void loadDefaultLayoutRules();
    
    // 重置引擎状态（清空布局规则并重新加载默认规则）
    void reset();
    
    ldt::CompiledLayoutRules compile(std::shared_ptr<ASTNode> ast) const;
private:
    // 解析 @layout 块中的布局规则
    void parseLayoutRules(std::shared_ptr<ASTNode> node, LayoutRuleBucket& bucket);
    
    // 解析单个布局规则节点
    void parseLayoutRule(std::shared_ptr<ASTNode> ruleNode, LayoutRuleBucket& bucket);
    
    // 检查选择器是否匹配节点
    bool matchesSelector(std::shared_ptr<ASTNode> node, const std::string& selector) const;
    
    // 添加默认布局规则（接受 Attribute map）
    void addDefaultLayoutRule(const std::string& selector, 
                             const std::map<std::string, Attribute>& properties);
    
    
    // 布局规则分桶存储
    LayoutRuleBucket defaultBucket_;
    LayoutRuleBucket globalBucket_;
    std::unordered_map<int, LayoutRuleBucket> localBuckets_;
    std::shared_ptr<ASTNode> lastAST_;   // 缓存上一次的 AST，用于增量更新
};
