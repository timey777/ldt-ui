#include "node_flags.h"
#include <array>
#include <sstream>
#include <string>
#include <limits>
#include <vector>

namespace ldt {

LayoutUnit LayoutUnit::fromString(const std::string& s) {
    if (s == "auto" || s.empty()) return autoUnit();
    if (s.back() == '%') {
        try { return percent(std::stof(s.substr(0, s.size() - 1))); }
        catch (...) { return autoUnit(); }
    }
    if (s.size() > 2 && s.substr(s.size() - 2) == "px") {
        try { return px(std::stof(s.substr(0, s.size() - 2))); }
        catch (...) { return autoUnit(); }
    }
    if (s.size() > 2 && s.substr(s.size() - 2) == "dp") {
        try { return dp(std::stof(s.substr(0, s.size() - 2))); }
        catch (...) { return autoUnit(); }
    }
    if (s.size() > 2 && s.substr(s.size() - 2) == "sp") {
        try { return sp(std::stof(s.substr(0, s.size() - 2))); }
        catch (...) { return autoUnit(); }
    }
    try { return px(std::stof(s)); }
    catch (...) { return autoUnit(); }
}

float LayoutUnit::resolve(float parentSize, bool parentIsDefinite) const {
    if (isAuto()) return AUTO_SENTINEL;
    if (isPercent()) {
        if (!parentIsDefinite) return AUTO_SENTINEL;
        return parentSize * value / 100.0f;
    }
    return value;
}

float LayoutUnit::resolveToPx(float parentSize, bool parentIsDefinite) const {
    float r = resolve(parentSize, parentIsDefinite);
    return (r == AUTO_SENTINEL) ? 0.0f : r;
}

bool tryParseAbsoluteLength(const std::string& s, float& outValue) {
    const LayoutUnit unit = LayoutUnit::fromString(s);
    if (!unit.isAbsoluteLength()) {
        return false;
    }
    outValue = unit.value;
    return true;
}

bool tryParseEdgeLengthShorthand(const std::string& s, ui::Edges& out,
                                 std::array<bool, 4>* outAuto) {
    std::stringstream ss(s);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token) tokens.push_back(token);
    if (tokens.empty()) return false;

    auto process = [&](const std::string& raw, float& value, bool& isAuto) {
        if (raw == "auto") {
            isAuto = true;
            value = 0.0f;
            return;
        }

        isAuto = false;
        if (!tryParseAbsoluteLength(raw, value)) {
            value = 0.0f;
        }
    };

    float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    bool autos[4] = {false, false, false, false};

    if (tokens.size() == 1) {
        process(tokens[0], values[0], autos[0]);
        values[1] = values[2] = values[3] = values[0];
        autos[1] = autos[2] = autos[3] = autos[0];
    } else if (tokens.size() == 2) {
        process(tokens[0], values[0], autos[0]);
        process(tokens[1], values[1], autos[1]);
        values[2] = values[0];
        values[3] = values[1];
        autos[2] = autos[0];
        autos[3] = autos[1];
    } else if (tokens.size() == 3) {
        process(tokens[0], values[0], autos[0]);
        process(tokens[1], values[1], autos[1]);
        process(tokens[2], values[2], autos[2]);
        values[3] = values[1];
        autos[3] = autos[1];
    } else {
        for (int index = 0; index < 4; ++index) {
            process(tokens[index], values[index], autos[index]);
        }
    }

    for (int index = 0; index < 4; ++index) {
        out[index] = values[index];
        if (outAuto) {
            (*outAuto)[index] = autos[index];
        }
    }

    return true;
}

FontWeight fontWeightFromString(const std::string& s) {
    if (s == "bold" || s == "700") return FontWeight::Bold;
    if (s == "100") return FontWeight::W100;
    if (s == "200") return FontWeight::W200;
    if (s == "300") return FontWeight::W300;
    if (s == "400") return FontWeight::W400;
    if (s == "500") return FontWeight::W500;
    if (s == "600") return FontWeight::W600;
    if (s == "800") return FontWeight::W800;
    if (s == "900") return FontWeight::W900;
    return FontWeight::Normal;
}

TextAlign textAlignFromString(const std::string& s) {
    if (s == "center") return TextAlign::Center;
    if (s == "right") return TextAlign::Right;
    if (s == "justify") return TextAlign::Justify;
    return TextAlign::Left;
}

Overflow overflowFromString(const std::string& s) {
    if (s == "hidden") return Overflow::Hidden;
    if (s == "scroll") return Overflow::Scroll;
    if (s == "auto") return Overflow::Auto;
    return Overflow::Visible;
}

BoxSizing boxSizingFromString(const std::string& s) {
    if (s == "border-box") return BoxSizing::BorderBox;
    return BoxSizing::ContentBox;
}

FormattingContext displayStringToEnum(const std::string& d) {
    if (d == "flex") return FormattingContext::Flex;
    if (d == "grid") return FormattingContext::Grid;
    if (d == "inline") return FormattingContext::Inline;
    if (d == "none") return FormattingContext::None;
    return FormattingContext::Block;
}

Position positionStringToEnum(const std::string& s) {
    if (s == "relative") return Position::Relative;
    if (s == "absolute") return Position::Absolute;
    if (s == "fixed") return Position::Fixed;
    return Position::Static;
}

FlexDirection flexDirectionStringToEnum(const std::string& s) {
    if (s == "column") return FlexDirection::Column;
    if (s == "row-reverse") return FlexDirection::RowReverse;
    if (s == "column-reverse") return FlexDirection::ColumnReverse;
    return FlexDirection::Row;
}

AlignItems alignItemsStringToEnum(const std::string& s) {
    if (s == "flex-start") return AlignItems::FlexStart;
    if (s == "flex-end") return AlignItems::FlexEnd;
    if (s == "center") return AlignItems::Center;
    if (s == "baseline") return AlignItems::Baseline;
    return AlignItems::Stretch;
}

JustifyContent justifyContentStringToEnum(const std::string& s) {
    if (s == "flex-end") return JustifyContent::FlexEnd;
    if (s == "center") return JustifyContent::Center;
    if (s == "space-between") return JustifyContent::SpaceBetween;
    if (s == "space-around") return JustifyContent::SpaceAround;
    if (s == "space-evenly") return JustifyContent::SpaceEvenly;
    return JustifyContent::FlexStart;
}

FlexWrap flexWrapStringToEnum(const std::string& s) {
    if (s == "wrap") return FlexWrap::Wrap;
    if (s == "wrap-reverse") return FlexWrap::WrapReverse;
    return FlexWrap::NoWrap;
}

std::string dirtyFlagToString(DirtyFlag flags) {
    if (flags == DirtyFlag::None) return "None";
    std::string s;
    if (hasDirtyFlag(flags, DirtyFlag::Layout)) s += "Layout|";
    if (hasDirtyFlag(flags, DirtyFlag::Style)) s += "Style|";
    if (hasDirtyFlag(flags, DirtyFlag::Paint)) s += "Paint|";
    if (hasDirtyFlag(flags, DirtyFlag::ChildrenLayout)) s += "ChildrenLayout|";
    if (!s.empty()) s.pop_back(); // remove trailing '|'
    return s;
}

} // namespace ldt
