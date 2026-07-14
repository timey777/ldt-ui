#pragma once

#include "engine.h"
#include "core/resolved_tree.h" // For UIState
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "core/node_flags.h"

namespace ldt { struct CompiledStyleRules; }

// 预编译选择器结构
struct CompiledSelector {
    enum class Type { Tag, Class, Id, Universal };
    Type type = Type::Universal;
    std::string value; // tag name, class name (without .), or id (without #)
};

// 将 state 字段加入 StyleRule，使规则可限定在特定 UI 状态下生效
struct StyleRule {
    std::string selector; // 原始选择器字符串

    CompiledSelector compiled;   // 预解析选择器（一次性）
    ldt::UIState requiredState = ldt::UIState::None;

    std::map<std::string, Attribute> properties; // 样式属性（直接存储 Attribute）

    bool affectsLayout = false;  // ⭐ 预计算：是否影响布局
    bool affectsPaint  = true;   // ⭐ 预计算：是否影响绘制
};

// 样式索引，用于快速查找特定状态下的规则
struct StyleIndex {
    std::vector<const StyleRule*> normal; // 无状态限制的规则
    std::vector<const StyleRule*> hover;
    std::vector<const StyleRule*> active;
    std::vector<const StyleRule*> focus;
    std::vector<const StyleRule*> dragging;
    std::vector<const StyleRule*> disabled;
};

// 样式规则桶：用于按来源分组存储规则（例如 default/global）
struct StyleRuleBucket {
    std::vector<StyleRule> rules;
    StyleIndex index;

    void clear() {
        rules.clear();
        index = StyleIndex{};
    }
};

enum class StyleRuleScope {
    Default,
    Global,
    Local
};

class StyleEngine : public AbstractEngine {
public:
    StyleEngine() = default;
    ~StyleEngine() = default;
    
    void initialize() override;
    void shutdown() override;
    std::type_index type() const override;
    void process(std::shared_ptr<ASTNode> root, ldt::DisplayList& outList,
                 float viewportW, float viewportH) override;
    // 当 AST 热重载或外部修改时调用，解析 @style 并触发必要的更新（例如重建 DisplayList）
    void onASTUpdated(std::shared_ptr<ASTNode> root) override;
    
    std::string name() const override;
    
    // 获取匹配节点的样式属性（返回 Attribute 指针）
    const Attribute* getStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property) const;
    // UIState-aware overload: only consider rules that either have state == UIState::None
    // or match the provided state (rules with matching state take precedence)
    const Attribute* getStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property, ldt::UIState state) const;
    
    // 获取样式属性的字符串值（便捷方法）
    std::string getStylePropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                      const std::string& defaultValue = "") const;
    // UIState-aware overload
    std::string getStylePropertyString(std::shared_ptr<ASTNode> node, const std::string& property,
                                      const std::string& defaultValue, ldt::UIState state) const;
    
    // 检查节点是否有样式属性
    bool hasStyleProperty(std::shared_ptr<ASTNode> node, const std::string& property) const;
    
    // 清空所有样式规则
    void clearStyles();
    
    // 加载默认样式
    void loadDefaultStyles();
    
    // 重置引擎状态（清空样式规则并重新加载默认样式）
    void reset();

    // 按作用域写入规则
    void setScopedRules(std::shared_ptr<ASTNode> root, StyleRuleScope scope,
                        std::optional<int> ownerId = std::nullopt, bool replace = true);
    // 清理作用域规则（Local 时 ownerId 必填）
    void clearScopedRules(StyleRuleScope scope, std::optional<int> ownerId = std::nullopt);

    // 将 AST 节点匹配到的规则预编译为合并后的样式（base + state deltas）
    ldt::CompiledStyleRules compile(std::shared_ptr<ASTNode> ast) const;

    static constexpr int ORDER_USER_START = 1000;
    static constexpr int ORDER_INLINE = 0x7FFFFFFF;
    
private:
    // 解析 @style 块中的样式规则
    void parseStyleRules(std::shared_ptr<ASTNode> node, StyleRuleBucket& bucket);
    
    // 解析单个样式规则节点
    void parseStyleRule(std::shared_ptr<ASTNode> ruleNode, StyleRuleBucket& bucket);
    
    // 检查选择器是否匹配节点
    bool matchesSelector(std::shared_ptr<ASTNode> node, const std::string& selector) const;
    
    // 添加默认样式规则（接受 Attribute map）
    void addDefaultStyleRule(const std::string& selector, 
                            const std::map<std::string, Attribute>& properties);

    // 重建单个桶索引
    void rebuildBucketIndex(StyleRuleBucket& bucket);

    // 根据节点解析本地样式 ownerId
    std::string getStyleOwnerId(std::shared_ptr<ASTNode> node) const;

    // 分桶存储
    StyleRuleBucket defaultBucket_;
    StyleRuleBucket globalBucket_;
    std::unordered_map<int, StyleRuleBucket> localBuckets_;
};
