#include "grid_layout.h"
#include "engine/core/property_resolver.h"
#include "engine/core/resolved_node.h"
#include <algorithm>
#include <cmath>
#include <sstream>

using namespace std;
using namespace ldt;

namespace {

// 布局过程中的轨道数据
struct GridLayoutData {
    std::vector<float> colSizes;
    std::vector<float> rowSizes;
    int numCols = 1;
    int numRows = 1;
};

static float clampf(float v, float lo, float hi) {
    if (!std::isnan(lo) && v < lo) v = lo;
    if (!std::isnan(hi) && v > hi) v = hi;
    return v;
}

static float toFloatOrZero(const std::string& s) {
    try { return std::stof(s); } catch (...) { return 0.0f; }
}

// 收集直接参与布局的子项（跳过 display:none）
static std::vector<ldt::ResolvedNode*> flowItems(ldt::ResolvedNode* node) {
    std::vector<ldt::ResolvedNode*> items;
    for (auto* ch : node->getFlowChildren()) {
        if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
        items.push_back(ch);
    }
    return items;
}

// 子项有效对齐：align-self（非 auto）覆盖容器 align-items（等价 CSS）
static ldt::AlignItems gridItemAlign(const IPropertyProvider& parentProp,
                                     const IPropertyProvider& childProp) {
    const ldt::AlignSelf as = childProp.getAlignSelf();
    if (as == ldt::AlignSelf::Auto) return parentProp.getAlignItems();
    switch (as) {
        case ldt::AlignSelf::Center:   return ldt::AlignItems::Center;
        case ldt::AlignSelf::FlexEnd:  return ldt::AlignItems::FlexEnd;
        case ldt::AlignSelf::FlexStart:return ldt::AlignItems::FlexStart;
        case ldt::AlignSelf::Baseline: return ldt::AlignItems::Baseline;
        case ldt::AlignSelf::Stretch:
        default:                       return ldt::AlignItems::Stretch;
    }
}

// 计算轨道布局数据（resolve/position 阶段复用）
//   containerW/H < 0 表示求内在尺寸（fr/% 轨道无法解析，取 0）
static GridLayoutData buildGridData(ldt::ResolvedNode* node,
                                    const std::vector<ldt::ResolvedNode*>& items,
                                    float containerW, float containerH) {
    const IPropertyProvider& prop = node->props();
    const float gap = prop.getGap();

    const auto cols = GridLayout::parseTracks(prop.getGridTemplateColumns());
    auto rows = GridLayout::parseTracks(prop.getGridTemplateRows());


    int numCols = static_cast<int>(cols.size());
    if (numCols <= 0) numCols = 1;

    const int itemCount = static_cast<int>(items.size());
    const int neededRows = (itemCount + numCols - 1) / numCols;  // ceil
    const int explicitRows = static_cast<int>(rows.size());
    const int numRows = std::max(explicitRows, neededRows);

    // 隐式行：显式行数不足时补 auto 轨道
    while (static_cast<int>(rows.size()) < numRows) rows.push_back(GridLayout::GridTrack{});

    // 每列 / 每行的内容尺寸（项需要多大空间）
    std::vector<float> colContent(numCols, 0.0f);
    std::vector<float> rowContent(numRows, 0.0f);
    for (int i = 0; i < itemCount; ++i) {
        const int r = i / numCols;
        const int c = i % numCols;
        colContent[c] = std::max(colContent[c], items[i]->layout.getMarginBox().width);
        rowContent[r] = std::max(rowContent[r], items[i]->layout.getMarginBox().height);
    }

    GridLayoutData data;
    data.numCols = numCols;
    data.numRows = numRows;
    data.colSizes = GridLayout::resolveTrackSizes(cols, containerW, colContent, gap);
    data.rowSizes = GridLayout::resolveTrackSizes(rows, containerH, rowContent, gap);
    return data;
}

static float sumWithGaps(const std::vector<float>& sizes, float gap) {
    float sum = 0.0f;
    for (float s : sizes) sum += s;
    if (!sizes.empty()) sum += gap * static_cast<float>(sizes.size() - 1);
    return sum;
}

} // namespace

// ─────────────────────────────────────────────────────────────
// 轨道解析
// ─────────────────────────────────────────────────────────────
std::vector<GridLayout::GridTrack> GridLayout::parseTracks(const std::string& s) {
    std::vector<GridTrack> out;
    std::stringstream stream(s);
    std::string token;
    while (stream >> token) {
        // 容忍 "1fr, 2fr" 写法（去掉尾逗号）
        if (!token.empty() && token.back() == ',') token.pop_back();
        if (token.empty()) continue;

        GridTrack t;
        if (token == "auto") {
            t.unit = TrackUnit::Auto;
        } else if (token.size() >= 2 && token.compare(token.size() - 2, 2, "fr") == 0) {
            t.unit = TrackUnit::Fr;
            t.value = toFloatOrZero(token.substr(0, token.size() - 2));
        } else if (!token.empty() && token.back() == '%') {
            t.unit = TrackUnit::Percent;
            t.value = toFloatOrZero(token.substr(0, token.size() - 1));
        } else if (token.size() >= 2 && token.compare(token.size() - 2, 2, "px") == 0) {
            t.unit = TrackUnit::Px;
            t.value = toFloatOrZero(token.substr(0, token.size() - 2));
        } else {
            // 裸数字按 px
            t.unit = TrackUnit::Px;
            t.value = toFloatOrZero(token);
        }
        out.push_back(t);
    }
    if (out.empty()) out.push_back(GridTrack{});  // 空模板 → 单个 auto 轨道
    return out;
}

// ─────────────────────────────────────────────────────────────
// 轨道尺寸解析：固定轨道先占，剩余空间按 fr 比例分配
// ─────────────────────────────────────────────────────────────
std::vector<float> GridLayout::resolveTrackSizes(
    const std::vector<GridTrack>& tracks,
    float containerContent,
    const std::vector<float>& contentPerTrack,
    float gap)
{
    const size_t n = tracks.size();
    std::vector<float> out(n, 0.0f);
    if (n == 0) return out;

    const bool intrinsic = !(containerContent >= 0.0f && std::isfinite(containerContent));
    float fixedSum = 0.0f;
    float totalFr = 0.0f;

    for (size_t i = 0; i < n; ++i) {
        const GridTrack& t = tracks[i];
        const float content = (i < contentPerTrack.size()) ? contentPerTrack[i] : 0.0f;
        switch (t.unit) {
        case TrackUnit::Px:
            out[i] = t.value;
            break;
        case TrackUnit::Percent:
            out[i] = intrinsic ? 0.0f : containerContent * t.value / 100.0f;
            break;
        case TrackUnit::Auto:
            out[i] = content;
            break;
        case TrackUnit::Fr:
            out[i] = 0.0f;
            totalFr += t.value;
            break;
        }
        if (t.unit != TrackUnit::Fr) fixedSum += out[i];
    }

    // 剩余空间按 fr 比例分配（内在尺寸阶段 fr 不参与，容器宽度由内容决定）
    if (totalFr > 0.0f && !intrinsic) {
        const float gaps = gap * static_cast<float>(n > 0 ? n - 1 : 0);
        float remaining = containerContent - fixedSum - gaps;
        if (remaining < 0.0f) remaining = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            if (tracks[i].unit == TrackUnit::Fr) {
                out[i] = remaining * (tracks[i].value / totalFr);
            }
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────
// measure 阶段：自底向上，量内在尺寸
// ─────────────────────────────────────────────────────────────
void GridLayout::measureGrid(BoxModelEngine* engine, ldt::ResolvedNode* node,
                             float availableForChildrenW, float availableForChildrenH,
                             float requestedW, float requestedH) {
    if (!node || !engine) return;
    const IPropertyProvider* prop = &node->props();

    // 先递归测量所有子项
    for (auto* ch : node->getFlowChildren()) {
        if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
        engine->measurePhase(ch, availableForChildrenW, availableForChildrenH,
                             prop->getDisplay(),
                             requestedW != ldt::AUTO_SENTINEL,
                             requestedH != ldt::AUTO_SENTINEL);
    }

    const std::vector<ldt::ResolvedNode*> items = flowItems(node);
    // 内在尺寸阶段：容器宽高未知，fr/% 取 0，px/auto 取内容
    const GridLayoutData data = buildGridData(node, items, -1.0f, -1.0f);
    const float intrinsicW = sumWithGaps(data.colSizes, prop->getGap());
    const float intrinsicH = sumWithGaps(data.rowSizes, prop->getGap());

    node->layout.intrinsicWidth = intrinsicW;
    node->layout.intrinsicHeight = intrinsicH;

    const float contentW = (requestedW != ldt::AUTO_SENTINEL) ? requestedW : intrinsicW;
    const float contentH = (requestedH != ldt::AUTO_SENTINEL) ? requestedH : intrinsicH;

    node->layout.computedWidth = clampf(contentW, node->layout.minWidth, node->layout.maxWidth);
    node->layout.computedHeight = clampf(contentH, node->layout.minHeight, node->layout.maxHeight);
}

// ─────────────────────────────────────────────────────────────
// resolve 阶段：自顶向下，父级已定尺寸，解析轨道 + stretch 定子项尺寸
// ─────────────────────────────────────────────────────────────
void GridLayout::resolveGrid(BoxModelEngine* engine, ldt::ResolvedNode* node) {
    if (!node || !engine) return;

    const std::vector<ldt::ResolvedNode*> items = flowItems(node);
    if (!items.empty()) {
        const float containerW = node->layout.getContentBox().width;
        const float containerH = node->layout.getContentBox().height;
        const GridLayoutData data = buildGridData(node, items, containerW, containerH);

        for (size_t i = 0; i < items.size(); ++i) {
            const int r = static_cast<int>(i) / data.numCols;
            const int c = static_cast<int>(i) % data.numCols;
            auto* child = items[i];
            const IPropertyProvider& childRes = child->props();

            bool changed = false;
            if (childRes.getWidth().isAuto()) {
                // stretch：内容尺寸 = cell 尺寸 - 自身 chrome（margin/border/padding）
                const float avail = data.colSizes[c]
                    - child->layout.margin.horizontal()
                    - child->layout.border.horizontal()
                    - child->layout.padding.horizontal();
                child->layout.computedWidth = clampf(std::max(0.0f, avail),
                                                     child->layout.minWidth,
                                                     child->layout.maxWidth);
                changed = true;
            }
            // 垂直拉伸仅在子项有效对齐为 stretch（默认）时进行；center/end 保留内容高度，由 position 阶段做行内偏移。
            // 子项 align-self（非 auto）覆盖容器 align-items。
            if (childRes.getHeight().isAuto()
                && gridItemAlign(node->props(), childRes) == ldt::AlignItems::Stretch) {
                const float avail = data.rowSizes[r]
                    - child->layout.margin.vertical()
                    - child->layout.border.vertical()
                    - child->layout.padding.vertical();
                child->layout.computedHeight = clampf(std::max(0.0f, avail),
                                                      child->layout.minHeight,
                                                      child->layout.maxHeight);
                changed = true;
            }
            if (changed) {
                child->layout.finalizeSizes();
                // 尺寸变化后重推子树（文本换行等依赖新尺寸）
                engine->reMeasureChildren(child, true, true);
            }
        }
    }

    // 递归解析子树
    for (auto* ch : node->getFlowChildren()) {
        if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
        engine->resolvePhase(ch);
    }
}

// ─────────────────────────────────────────────────────────────
// position 阶段：只定位子项（按 cell），不做尺寸解析
// ─────────────────────────────────────────────────────────────
void GridLayout::positionGrid(BoxModelEngine* engine, ldt::ResolvedNode* node,
                              float contentAbsoluteX, float contentAbsoluteY) {
    if (!node || !engine) return;
    const IPropertyProvider* prop = &node->props();

    const std::vector<ldt::ResolvedNode*> items = flowItems(node);
    if (items.empty()) return;

    const float containerW = node->layout.getContentBox().width;
    const float containerH = node->layout.getContentBox().height;
    const GridLayoutData data = buildGridData(node, items, containerW, containerH);

    const float gap = prop->getGap();
    float y = contentAbsoluteY;
    for (int r = 0; r < data.numRows; ++r) {
        float x = contentAbsoluteX;
        for (int c = 0; c < data.numCols; ++c) {
            const int idx = r * data.numCols + c;
            if (idx >= static_cast<int>(items.size())) break;
            // 子项有效对齐（align-self 覆盖父 align-items）：非 stretch 时保留内容高度，在行内做垂直偏移（center/flex-end）
            float yOffset = 0.0f;
            const ldt::AlignItems itemAlign = gridItemAlign(*prop, items[idx]->props());
            if (itemAlign != ldt::AlignItems::Stretch) {
                const float itemH = items[idx]->layout.getMarginBox().height;
                if (itemAlign == ldt::AlignItems::Center) {
                    yOffset = (data.rowSizes[r] - itemH) * 0.5f;
                } else if (itemAlign == ldt::AlignItems::FlexEnd) {
                    yOffset = data.rowSizes[r] - itemH;
                }
                if (yOffset < 0.0f) yOffset = 0.0f;
            }
            engine->layoutPhase(items[idx], x, y + yOffset);
            x += data.colSizes[c] + gap;
        }
        y += data.rowSizes[r] + gap;
    }
}
