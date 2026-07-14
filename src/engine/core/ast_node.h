#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "attribute.h"
#include "ldt_export.h"
#include <optional>
// ------------------- AST & Visitor -------------------
struct LDT_API ASTNode {
public:
    // 使用 Attribute 存储属性，便于表达多种类型（字符串/数字/数组/对象等）
    std::unordered_map<std::string, Attribute> attrs;
    std::optional<int> localScopeId;
public:
    std::string type; // e.g. "Root", "Import", "Panel", "StyleRule"
    std::vector<std::shared_ptr<ASTNode>> children;
    // 存放解析时分离出的引擎注解块，例如 @style/@layout/@script 等
    // key 通常为原始关键字（例如 "@style"），value 列表为对应解析出来的 AST 节点
    std::unordered_map<std::string, std::vector<std::shared_ptr<ASTNode>>> meta;

    // 父节点指针 (weak_ptr 防止循环引用)
    std::weak_ptr<ASTNode> parent;

    // 支持 classList
    std::vector<std::string> classList;

    // 内部持久化计数器，用于生成唯一的子节点路径索引
    // key 为 child.type (如 "panel"), value 为已使用的最大索引
    std::unordered_map<std::string, int> childTypeCounters;

    void setClassList(const std::vector<std::string>& classes);
    const std::vector<std::string>& getClassList() const;
    // helper factory
    static std::shared_ptr<ASTNode> make(const std::string& t);
    ASTNode() = default;
    explicit ASTNode(const std::string& t);

    // 按需将另一个 AST 的内容合并到当前节点（就地更新，尽量保留原节点指针）
    void updateFrom(const std::shared_ptr<ASTNode>& other);

    // 将节点加入 meta 存储（用于顶层 Root 节点保存多个引擎块）
    void addMeta(const std::string& key, std::shared_ptr<ASTNode> node);

    // 获取 meta 条目（若不存在返回 nullptr）
    const std::vector<std::shared_ptr<ASTNode>>* getMeta(const std::string& key) const;

    bool hasMeta(const std::string& key) const;

    const Attribute* getUid() const;
    // 获取属性对象
    const Attribute* getAttribute(const std::string& key) const;

    // 检查是否有某个属性
    bool hasAttribute(const std::string& key) const;

    bool setAttribute(const std::string& key, const Attribute& attr);

    // 判断是否有某个 class（用于样式选择器）
    bool hasClass(const std::string& cls) const;
    void setLocalScopeId(std::optional<int> id);
    const std::optional<int> getLocalScopeId();
};
