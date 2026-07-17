#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <cmath>
#include "components/uitypes.h"

namespace ldt {

enum class FormattingContext {
    None,
    Inline,
    Block,
    Flex,
    Grid
};

static constexpr float AUTO_SENTINEL = -1.0f;
static constexpr float UNBOUNDED = std::numeric_limits<float>::infinity();

inline bool isAuto(float v) { return v == AUTO_SENTINEL; }
inline bool isDefinite(float v) { return v >= 0.0f && std::isfinite(v); }

enum class Unit : uint8_t {
    Px,
    Dp,
    Sp,
    Percent,
    Auto
};

struct LayoutUnit {
    float value = 0.0f;
    Unit unit = Unit::Auto;

    bool isAuto() const { return unit == Unit::Auto; }
    bool isPercent() const { return unit == Unit::Percent; }
    bool isPx() const { return unit == Unit::Px; }
    bool isDp() const { return unit == Unit::Dp; }
    bool isSp() const { return unit == Unit::Sp; }
    bool isAbsoluteLength() const { return isPx() || isDp() || isSp(); }

    static LayoutUnit px(float v) { return { v, Unit::Px }; }
    static LayoutUnit dp(float v) { return { v, Unit::Dp }; }
    static LayoutUnit sp(float v) { return { v, Unit::Sp }; }
    static LayoutUnit percent(float v) { return { v, Unit::Percent }; }
    static LayoutUnit autoUnit() { return { 0.0f, Unit::Auto }; }

    static LayoutUnit fromString(const std::string& s);

    float resolve(float parentSize, bool parentIsDefinite = true) const;

    float resolveToPx(float parentSize, bool parentIsDefinite = true) const;
};

// UIUnit is a simpler alternative for properties like border-radius that primary use pixels
struct UIUnit {
    float value = 0.0f;

    static UIUnit px(float v) { return { v }; }
    static UIUnit fromString(const std::string& s) {
        LayoutUnit unit = LayoutUnit::fromString(s);
        if (unit.isAuto() || unit.isPercent()) return { 0.0f };
        return { unit.value };
    }
    float toPx() const { return value; }
};

bool tryParseAbsoluteLength(const std::string& s, float& outValue);
bool tryParseEdgeLengthShorthand(const std::string& s, ui::Edges& out,
                                 std::array<bool, 4>* outAuto = nullptr);

enum class FlexDirection : uint8_t { Row, Column, RowReverse, ColumnReverse };
enum class AlignItems : uint8_t { Stretch, FlexStart, FlexEnd, Center, Baseline };
enum class JustifyContent : uint8_t { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class FlexWrap : uint8_t { NoWrap, Wrap, WrapReverse };
enum class BoxSizing : uint8_t { ContentBox, BorderBox };
enum class Overflow : uint8_t { Visible, Hidden, Scroll, Auto };
enum class Position : uint8_t { Static, Relative, Absolute, Fixed };

enum class FontWeight : uint8_t {
    Normal,
    Bold,
    W100, W200, W300, W400, W500, W600, W700, W800, W900
};

enum class TextAlign : uint8_t {
    Left,
    Center,
    Right,
    Justify
};

FontWeight fontWeightFromString(const std::string& s);
TextAlign textAlignFromString(const std::string& s);
Overflow overflowFromString(const std::string& s);
BoxSizing boxSizingFromString(const std::string& s);
FormattingContext displayStringToEnum(const std::string& d);
Position positionStringToEnum(const std::string& s);
FlexDirection flexDirectionStringToEnum(const std::string& s);
AlignItems alignItemsStringToEnum(const std::string& s);
JustifyContent justifyContentStringToEnum(const std::string& s);
FlexWrap flexWrapStringToEnum(const std::string& s);

// ================= UI 状态 =================
enum class UIState : uint8_t {
    None = 0,
    Hover = 1 << 0,
    Active = 1 << 1,
    Focus = 1 << 2,
    Dragging = 1 << 3,
    Disabled = 1 << 4
};

inline UIState operator|(UIState a, UIState b) {
    return static_cast<UIState>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline UIState operator&(UIState a, UIState b) {
    return static_cast<UIState>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool hasUIState(UIState s, UIState test) {
    return (static_cast<uint8_t>(s) & static_cast<uint8_t>(test)) != 0;
}

// ================= 脏标记 =================
enum class DirtyFlag : uint32_t {
    None = 0,
    Layout = 1 << 0,
    Style = 1 << 1,
    Paint = 1 << 2,
    ChildrenLayout = 1 << 3,
    Removed = 1 << 4,
    All = Layout | Style | Paint | ChildrenLayout
};

inline DirtyFlag operator|(DirtyFlag a, DirtyFlag b) {
    return static_cast<DirtyFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline DirtyFlag operator&(DirtyFlag a, DirtyFlag b) {
    return static_cast<DirtyFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline DirtyFlag& operator|=(DirtyFlag& a, DirtyFlag b) {
    a = a | b;
    return a;
}

inline bool hasDirtyFlag(DirtyFlag flags, DirtyFlag check) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(check)) != 0;
}

// 调试辅助：将 DirtyFlag 转换为可读字符串
std::string dirtyFlagToString(DirtyFlag flags);

} // namespace ldt
