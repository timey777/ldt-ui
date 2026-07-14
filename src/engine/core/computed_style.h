#pragma once

#include "render/display_list.h"
#include "node_flags.h"
#include <string>
#include <array>
#include <bitset>
#include <vector>

namespace ldt {

enum class Anchor {
    None,
    TopLeft,
    Center,
    TopRight,
    BottomLeft,
    BottomRight
};

Anchor stringToAnchor(const std::string &str);

enum class StyleProp : uint32_t {
    BackgroundColor = 0,
    TextColor,
    BorderColor,
    BorderWidth,
    BorderRadius,
    FontSize,
    LineHeight,
    FontFamily,
    FontWeight,
    TextAlign,
    Width,
    Height,
    Overflow,
    BoxSizing,
    Padding,
    Margin,
    MarginAuto,
    Opacity,
    Visible,
    Overlay,
    Anchor,
    OffsetX,
    OffsetY,
    BoxShadow,
    BackgroundImage,
    Count
};

struct ComputedStyle {
    ui::Color backgroundColor{0, 0, 0, 0};
    ui::Color textColor{0, 0, 0, 1};
    ui::Color borderColor{0, 0, 0, 0};
    ui::Edges borderWidth = {0.0f, 0.0f, 0.0f, 0.0f};
    UIUnit borderRadius = UIUnit::px(0.0f);
    float fontSize = 14.0f;
    float lineHeight = 0.0f;
    std::string fontFamily;
    FontWeight fontWeight = FontWeight::Normal;
    TextAlign textAlign = TextAlign::Left;
    LayoutUnit width = LayoutUnit::autoUnit();
    LayoutUnit height = LayoutUnit::autoUnit();
    Overflow overflow = Overflow::Auto;
    BoxSizing boxSizing = BoxSizing::ContentBox;
    ui::Edges padding = {0, 0, 0, 0};
    ui::Edges margin = {0, 0, 0, 0};
    std::array<bool, 4> marginAuto = {false, false, false, false};

    float opacity = 1.0f;
    bool visible = true;
    bool overlay = false;
    Anchor anchor = Anchor::None;
    float offsetX = 0, offsetY = 0;
    std::string boxShadow = "";
    std::string backgroundImage = "";
};

/**
 * @brief 表示一组样式声明的增量（原 StyleDelta）
 */
struct StyleDeclaration : public ComputedStyle {
    std::bitset<static_cast<size_t>(StyleProp::Count)> hasValue;

    bool has(StyleProp prop) const {
        return hasValue.test(static_cast<size_t>(prop));
    }

    void set(StyleProp prop) {
        hasValue.set(static_cast<size_t>(prop));
    }

    void clear(StyleProp prop) {
        hasValue.reset(static_cast<size_t>(prop));
    }
};

// 保持兼容性别名
using StyleDelta = StyleDeclaration;

/**
 * @brief 封装特定状态下的样式规则逻辑
 */
struct StateRule {
    StyleDeclaration delta;
    bool hasEffect = false;
    bool affectsLayout = false;
    bool affectsPaint = false;
};

/**
 * @brief 封装样式层叠和物理顺序逻辑
 */
struct StyleCascader {
    struct Entry {
        ldt::UIState requiredState = ldt::UIState::None;
        ldt::StyleDeclaration delta;
        bool affectsLayout = false;
        bool affectsPaint = true;
        int order = 0;
    };

    // 所有按源码顺序匹配的规则
    std::vector<Entry> orderedRules;

    // 基础样式（UIState::None）中各属性对应的最大 Order
    // 使用应用层定义的 StyleProp 枚举作为索引，替代字符串 Map，性能更高
    std::array<int, static_cast<size_t>(StyleProp::Count)> basePropertyOrders;

    StyleCascader() {
        basePropertyOrders.fill(-1);
    }

    void updateBaseOrder(StyleProp prop, int order) {
        int& current = basePropertyOrders[static_cast<size_t>(prop)];
        if (order > current) current = order;
    }

    bool canOverride(StyleProp prop, int currentOrder) const {
        return currentOrder >= basePropertyOrders[static_cast<size_t>(prop)];
    }
};

/**
 * @brief 预编译后的规则集合（挂载到 ResolvedNode 上，便于后续快速合并）
 */
struct CompiledStyleRules {
    ldt::ComputedStyle base;		 // rules with UIState::None merged

    // 状态快速索引映射 (用于优化判定)
    enum StateIndex {
        Hover = 0,
        Active,
        Focus,
        Dragging,
        Disabled,
        Count
    };
    
    std::array<StateRule, StateIndex::Count> stateRules;

    // 层叠逻辑封装
    StyleCascader cascader;

    // 性能优化标记：指示是否有任何状态增量会实际影响布局/绘画
    bool hasStatefulLayout = false;
    bool hasStatefulPaint = false;

    CompiledStyleRules() = default;
};

} // namespace ldt
