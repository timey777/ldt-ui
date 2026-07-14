#pragma once
#include <vector>
#include <functional>
#include <string>
#include <map>
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_node.h"
namespace ldt {
class LDT_API UIEventSystem {
public:
    UIEventSystem() = default;
    ~UIEventSystem() = default;

    void updateHover(ResolvedNode* newHover);
    void updateActive(ResolvedNode* newActive);
    void updateFocus(ResolvedNode* newFocus);
    ResolvedNode* getFocusNode();
    // 移除节点时清空相关引用
    void notifyNodeRemoved(ResolvedNode* node);
    // 返回当前 hover 路径的最深节点（如果没有 hover 则返回 nullptr）
    ResolvedNode* getHoverNode() const;

    // ----------------- Event binding API -----------------
    enum class UIEventType {
        Click,
        MouseDown,
        MouseUp,
        Input,
        Change,
        Scroll,
        Focus,
        Blur,
        KeyDown,
        KeyUp
    };

    struct UIEvent {
        UIEventType type;
        // event target control
        ldt::AbstractControl* target = nullptr;
        bool stopPropagation = false;

        // optional payload
        std::string text;
        int key = 0;
    };

    // Selector (supports #id and .class)
    struct Selector {
        enum class Type { Id, Class };
        Type type;
        std::string value;
    };

    // parse simple selector string (#id or .class)
    static Selector parseSelector(const std::string& s);

    using EventCallback = std::function<void(const UIEvent&)>;

    // bind a handler
    void bind(const std::string& selector, UIEventType type, EventCallback cb);

    // dispatch an event (called by Scene)
    void dispatch(const UIEvent& e);
    void flushPendingHandlers();
private:

    struct Handler {
        Selector selector;
        UIEventType type;
        EventCallback cb;
    };

    std::vector<Handler> handlers_;
    std::vector<Handler> pendingHandlers_;
    int dispatchDepth_ = 0;


    // store ancestor paths (root -> node) for hover/active so we can add/remove state along the path
    std::vector<ResolvedNode*> hoverPath_;
    std::vector<ResolvedNode*> activePath_;
    ResolvedNode* focusNode_ = nullptr;
};

} // namespace ldt
