#include "engine/core/resolved_node.h"
// 单个节点的视图基类
template<typename Derived>
class NodeViewBase {
protected:
    ResolvedNode* node;

public:
    explicit NodeViewBase(ResolvedNode* node = nullptr)
        : node(node) {
    }

    // 基础访问
    operator ResolvedNode* () const { return node; }
    ResolvedNode* operator->() const { return node; }
    ResolvedNode* get() const { return node; }

    // 获取子节点视图容器
    class ChildrenView {
    private:
        ResolvedNode* parentNode;
        const Derived& viewPrototype;

    public:
        ChildrenView(ResolvedNode* parent, const Derived& proto)
            : parentNode(parent), viewPrototype(proto) {
        }

        // 迭代器返回当前类型的视图
        struct Iterator {
            size_t i;
            ResolvedNode* parent;
            const Derived& viewPrototype;

            void skip() {
                auto& children = parent->children;  // 假设ResolvedNode有children成员
                while (i < children.size() && !viewPrototype.shouldInclude(children[i]))
                    ++i;
            }

            // 关键：返回当前类型的视图实例
            Derived operator*() const {
                return Derived{ parent->children[i] };
            }

            Iterator& operator++() {
                ++i;
                skip();
                return *this;
            }

            bool operator!=(const Iterator& o) const {
                return i != o.i;
            }
        };

        Iterator begin() const {
            Iterator it{ 0, parentNode, viewPrototype };
            it.skip();
            return it;
        }

        Iterator end() const {
            return Iterator{ parentNode->children.size(), parentNode, viewPrototype };
        }

        // 转换为vector
        std::vector<Derived> toVector() const {
            std::vector<Derived> result;
            for (auto view : *this) {
                result.push_back(view);
            }
            return result;
        }

        // 检查是否为空
        bool empty() const {
            return begin() == end();
        }

        // 获取数量
        size_t size() const {
            size_t count = 0;
            for (auto it = begin(); it != end(); ++it) ++count;
            return count;
        }
    };

    // 获取子视图容器
    ChildrenView getChildren() const {
        return ChildrenView{ node, static_cast<const Derived&>(*this) };
    }

    // 获取所有符合条件的后代节点
    template<typename OutputIterator>
    void getAllDescendants(OutputIterator out) const {
        for (auto child : getChildren()) {
            *out++ = child;
            child.getAllDescendants(out);
        }
    }

    // 查找满足条件的后代节点
    template<typename Predicate>
    Derived findDescendant(Predicate pred) const {
        for (auto child : getChildren()) {
            if (pred(child)) return child;
            if (auto found = child.findDescendant(pred)) {
                return found;
            }
        }
        return Derived{ nullptr };
    }

    // 子类必须实现的条件判断
    virtual bool shouldInclude(ResolvedNode* node) const = 0;

    // 转换为其他视图类型
    template<typename OtherView>
    OtherView as() const {
        return OtherView{ node };
    }

    // 检查有效性
    bool isValid() const { return node != nullptr; }
    explicit operator bool() const { return isValid(); }

    // 比较操作
    bool operator==(const Derived& other) const {
        return node == other.node;
    }
    bool operator!=(const Derived& other) const {
        return node != other.node;
    }
};