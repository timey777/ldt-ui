#include "flex_layout.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include "engine/core/resolved_node.h"
#include "engine/core/property_resolver.h"
#include "misc/float_utils.h"
using namespace std;
using namespace ldt;



static float clampf_flex(float v, float lo, float hi) {
    if (!isnan(lo) && v < lo) v = lo;
    if (!isnan(hi) && v > hi) v = hi;
    return v;
}

bool FlexLayout::isRowDirection(ldt::FlexDirection flexDirection) {
    return flexDirection == ldt::FlexDirection::Row || flexDirection == ldt::FlexDirection::RowReverse;
}

bool FlexLayout::isReverseDirection(ldt::FlexDirection flexDirection) {
    return flexDirection == ldt::FlexDirection::RowReverse || flexDirection == ldt::FlexDirection::ColumnReverse;
}

float FlexLayout::getMainSize(const ldt::ResolvedNode* node, bool isRow) {
    return isRow ? node->layout.getMarginBox().width : node->layout.getMarginBox().height;
}

float FlexLayout::getCrossSize(const ldt::ResolvedNode* node, bool isRow) {
    return isRow ? node->layout.getMarginBox().height : node->layout.getMarginBox().width;
}

static float getFlexMinMainContentSize(const ldt::ResolvedNode* node, bool isRow) {
    return isRow ? node->layout.minWidth : node->layout.minHeight;
}

static float getFlexMaxMainContentSize(const ldt::ResolvedNode* node, bool isRow) {
    return isRow ? node->layout.maxWidth : node->layout.maxHeight;
}

static float getComputedMainContentSize(const ldt::ResolvedNode* node, bool isRow) {
    return isRow ? node->layout.computedWidth : node->layout.computedHeight;
}

static void setComputedMainContentSize(ldt::ResolvedNode* node, bool isRow, float value) {
    if (isRow) node->layout.computedWidth = value;
    else node->layout.computedHeight = value;
    node->layout.finalizeSizes();
}

float FlexLayout::getMarginMain(const ldt::ResolvedNode* node, bool isRow) {
    return 0.0f;
}

float FlexLayout::getMarginCross(const ldt::ResolvedNode* node, bool isRow) {
    return 0.0f;
}

void FlexLayout::collectFlexLines(ldt::ResolvedNode* node,
                                 float availableMain, 
                                 bool isRow, 
                                 bool isWrap, 
                                 float gap,
                                 std::vector<FlexLine>& outLines) {
    FlexLine currentLine;
    float currentMainSize = 0;
    for (size_t i = 0; i < node->getFlowChildren().size(); ++i) {
        auto& child = node->getFlowChildren()[i];
        if (child->props().getDisplay() == ldt::FormattingContext::None) continue;
        float childMain = getMainSize(child, isRow);
        float childCross = getCrossSize(child, isRow);


        float gapSize = (currentLine.childIndices.empty()) ? 0 : gap;

        if (isWrap && !currentLine.childIndices.empty() &&
            (ldt::floatGreater(currentMainSize + gapSize + childMain, availableMain)) && availableMain > 0) {
            outLines.push_back(currentLine);
            currentLine = FlexLine();
            currentMainSize = 0;
            gapSize = 0;
        }

        currentLine.childIndices.push_back(i);
        currentMainSize += gapSize + childMain;
        currentLine.mainSize = currentMainSize;
        currentLine.crossSize = std::max(currentLine.crossSize, childCross);
    }
    if (!currentLine.childIndices.empty() || !node->getFlowChildren().size()) {
        outLines.push_back(currentLine);
    }
}

void FlexLayout::measureFlex(BoxModelEngine* engine, ldt::ResolvedNode* node,
                             float availableForChildrenW, float availableForChildrenH,
                             float requestedW, float requestedH) {
    if (!node) return;

    const IPropertyProvider* prop = &node->props();
    bool isRow = isRowDirection(prop->getFlexDirection());

    bool isWrap = prop->getFlexWrap() != ldt::FlexWrap::NoWrap;
    float gap = prop->getGap();

    for (auto* ch : node->getFlowChildren()) {
        if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
        engine->measurePhase(ch, availableForChildrenW, availableForChildrenH, prop->getDisplay(), requestedW != ldt::AUTO_SENTINEL, requestedH != ldt::AUTO_SENTINEL);
    }


    float availableMain = isRow ? availableForChildrenW : availableForChildrenH;
    std::vector<FlexLine> lines;
    collectFlexLines(node, availableMain, isRow, isWrap, gap, lines);

    float totalMain = 0;
    float totalCross = 0;
    float maxLineMain = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        maxLineMain = std::max(maxLineMain, lines[i].mainSize);
        totalCross += lines[i].crossSize;
        if (i > 0) totalCross += gap; 
    }
    
    float contentW = isRow ? maxLineMain : totalCross;
    float contentH = isRow ? totalCross : maxLineMain;

    // 内在（内容）尺寸：与最终尺寸分离，供 resolve 阶段 / 调试使用。
    // 这里记录的是“内容自然需要多大”，不受父级 definite 约束覆盖。
    node->layout.intrinsicWidth = contentW;
    node->layout.intrinsicHeight = contentH;

    if (requestedW != ldt::AUTO_SENTINEL) {
        contentW = requestedW;
    }
    // 宽度为 auto：保持按子项算出的内在尺寸（intrinsicWidth），不做块级 fill。
    // 父级宽度只是约束；最终宽度由父级 flex 在 resolve 阶段的 grow/shrink 决定。

    
    if (requestedH != ldt::AUTO_SENTINEL) contentH = requestedH;

    node->layout.computedWidth = clampf_flex(contentW, node->layout.minWidth, node->layout.maxWidth);
    node->layout.computedHeight = clampf_flex(contentH, node->layout.minHeight, node->layout.maxHeight);
}

// ─────────────────────────────────────────────────────────────────────────
// resolve 阶段：自顶向下，只解析子项【最终尺寸】。
//   · 主轴：grow / shrink（flex base = 子项当前尺寸 = 测量结果或确定尺寸）
//   · 交叉轴：line cross（可拉大可压缩）+ stretch
//   · 不做任何定位；子项尺寸定完后递归 resolve 子树
// ─────────────────────────────────────────────────────────────────────────
void FlexLayout::resolveFlex(BoxModelEngine* engine, ldt::ResolvedNode* node) {
    if (!node) return;

    const IPropertyProvider* prop = &node->props();
    bool isRow = isRowDirection(prop->getFlexDirection());

    bool isWrap = prop->getFlexWrap() != ldt::FlexWrap::NoWrap;
    float gap = prop->getGap();
    
    float containerMain = isRow ? node->layout.computedWidth : node->layout.computedHeight;
    float containerCross = isRow ? node->layout.computedHeight : node->layout.computedWidth;
    
    std::vector<FlexLine> lines;
    collectFlexLines(node, containerMain, isRow, isWrap, gap, lines);

    for (auto& line : lines) {
        float usedMain = line.mainSize;
        float freeMain = containerMain - usedMain;

        auto refreshLine = [&]() {
            line.mainSize = 0.0f;
            line.crossSize = 0.0f;
            for (size_t idx : line.childIndices) {
                auto& child = node->getFlowChildren()[idx];
                line.mainSize += getMainSize(child, isRow);
                line.crossSize = std::max(line.crossSize, getCrossSize(child, isRow));
            }
            if (line.childIndices.size() > 1) {
                line.mainSize += gap * static_cast<float>(line.childIndices.size() - 1);
            }
            usedMain = line.mainSize;
            freeMain = containerMain - usedMain;
        };

        // ── 主轴解析：grow / shrink ──
        if (freeMain > 0.0f) {
            for (size_t pass = 0; pass < line.childIndices.size() && freeMain > 0.0f; ++pass) {
                float totalFlexGrow = 0.0f;
                for (size_t idx : line.childIndices) {
                    auto& child = node->getFlowChildren()[idx];
                    const float grow = std::max(0.0f, child->props().getFlexGrow());
                    if (grow > 0.0f &&
                        ldt::floatGreater(getFlexMaxMainContentSize(child, isRow),
                                          getComputedMainContentSize(child, isRow))) {
                        totalFlexGrow += grow;
                    }
                }
                if (totalFlexGrow <= 0.0f) break;

                const float spaceToDistribute = freeMain;
                float addedSpaceTotal = 0.0f;
                for (size_t idx : line.childIndices) {
                    auto& child = node->getFlowChildren()[idx];
                    const float grow = std::max(0.0f, child->props().getFlexGrow());
                    const float current = getComputedMainContentSize(child, isRow);
                    const float maximum = getFlexMaxMainContentSize(child, isRow);
                    if (grow <= 0.0f || !ldt::floatGreater(maximum, current)) continue;

                    const float share = (grow / totalFlexGrow) * spaceToDistribute;
                    const float next = std::min(maximum, current + share);
                    const float added = next - current;
                    if (added <= 0.0f) continue;

                    setComputedMainContentSize(child, isRow, next);
                    // flex-grow only resolves the main axis; the cross axis is re-derived
                    // from content by reMeasureChildren if it is auto.
                    engine->reMeasureChildren(child, isRow, !isRow);
                    addedSpaceTotal += added;
                }
                if (addedSpaceTotal <= 0.0f) break;
                refreshLine();
            }
        }

        if (freeMain < 0.0f) {
            for (size_t pass = 0; pass < line.childIndices.size() && freeMain < 0.0f; ++pass) {
                float totalShrink = 0.0f;
                for (size_t idx : line.childIndices) {
                    auto& child = node->getFlowChildren()[idx];
                    const float shrink = std::max(0.0f, child->props().getFlexShrink());
                    const float minimum = getFlexMinMainContentSize(child, isRow);
                    if (shrink > 0.0f &&
                        ldt::floatGreater(getComputedMainContentSize(child, isRow), minimum)) {
                        totalShrink += shrink;
                    }
                }
                if (totalShrink <= 0.0f) break;

                const float overflowMain = -freeMain;
                float reducedSpaceTotal = 0.0f;
                for (size_t idx : line.childIndices) {
                    auto& child = node->getFlowChildren()[idx];
                    const float shrink = std::max(0.0f, child->props().getFlexShrink());
                    const float current = getComputedMainContentSize(child, isRow);
                    const float minimum = getFlexMinMainContentSize(child, isRow);
                    if (shrink <= 0.0f || !ldt::floatGreater(current, minimum)) continue;

                    const float reduction = overflowMain * (shrink / totalShrink);
                    const float next = std::max(minimum, current - reduction);
                    const float reduced = current - next;
                    if (reduced <= 0.0f) continue;

                    setComputedMainContentSize(child, isRow, next);
                    // flex-shrink only resolves the main axis; the cross axis is re-derived
                    // from content by reMeasureChildren if it is auto.
                    engine->reMeasureChildren(child, isRow, !isRow);
                    reducedSpaceTotal += reduced;
                }
                if (reducedSpaceTotal <= 0.0f) break;
                refreshLine();
            }
        }

        // ── 交叉轴解析：单行 line cross = 容器交叉尺寸（父级已解析/确定），stretch 定子项交叉尺寸 ──
        ldt::AlignItems alignItems = prop->getAlignItems();

        float lineCrossSize = (lines.size() == 1) ? containerCross : line.crossSize;

        for (size_t i = 0; i < line.childIndices.size(); ++i) {
            size_t idx = line.childIndices[i];
            auto& child = node->getFlowChildren()[idx];

            if (alignItems == ldt::AlignItems::Stretch) {
                const IPropertyProvider& childRes = child->props();
                if (isRow) {
                    if (childRes.getDisplay() != ldt::FormattingContext::Inline && childRes.getHeight().isAuto()) {
                        // overflow:visible 的子项不被压缩：保留内容尺寸并把溢出交给父级
                        float targetCross = (childRes.getOverflow() != ldt::Overflow::Visible)
                            ? lineCrossSize
                            : std::max(lineCrossSize, child->layout.intrinsicHeight);
                        float availableForContent = targetCross - child->layout.margin.vertical() - child->layout.border.vertical() - child->layout.padding.vertical();
                        child->layout.computedHeight = clampf_flex(
                            std::max(0.0f, availableForContent),
                            child->layout.minHeight,
                            child->layout.maxHeight);
                        child->layout.finalizeSizes();
                        // Re-measure children of this item because its size changed (stretch).
                        // Both axes are authoritative here: width was resolved by the flex
                        // algorithm and height was just set by stretch.
                        engine->reMeasureChildren(child, true, true);
                    }
                } else {
                    if (childRes.getDisplay() != ldt::FormattingContext::Inline && childRes.getWidth().isAuto()) {
                        // overflow:visible 的子项不被压缩：保留内容尺寸并把溢出交给父级
                        float targetCross = (childRes.getOverflow() != ldt::Overflow::Visible)
                            ? lineCrossSize
                            : std::max(lineCrossSize, child->layout.intrinsicWidth);
                        float availableForContent = targetCross - child->layout.margin.horizontal() - child->layout.border.horizontal() - child->layout.padding.horizontal();
                        child->layout.computedWidth = clampf_flex(
                            std::max(0.0f, availableForContent),
                            child->layout.minWidth,
                            child->layout.maxWidth);
                        child->layout.finalizeSizes();
                        // Re-measure children of this item because its size changed (stretch).
                        // Both axes are authoritative here: height was resolved by the flex
                        // algorithm and width was just set by stretch.
                        engine->reMeasureChildren(child, true, true);
                    }
                }
            }
        }
    }

    // 递归：所有子项尺寸已解析，继续解析它们的子树
    for (auto* ch : node->getFlowChildren()) {
        if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
        engine->resolvePhase(ch);
    }
}

// ─────────────────────────────────────────────────────────────────────────
// position 阶段：只定位子项并递归，不做任何尺寸解析。
// 所有子项尺寸在 resolve 阶段已定，这里只摆 x/y（含 justify / align 偏移）。
// ─────────────────────────────────────────────────────────────────────────
void FlexLayout::positionFlex(BoxModelEngine* engine, ldt::ResolvedNode* node,
                              float contentAbsoluteX, float contentAbsoluteY) {
    if (!node) return;

    const IPropertyProvider* prop = &node->props();
    bool isRow = isRowDirection(prop->getFlexDirection());

    bool reverse = isReverseDirection(prop->getFlexDirection());
    bool isWrap = prop->getFlexWrap() != ldt::FlexWrap::NoWrap;
    float gap = prop->getGap();
    
    float containerMain = isRow ? node->layout.computedWidth : node->layout.computedHeight;
    float containerCross = isRow ? node->layout.computedHeight : node->layout.computedWidth;
    
    std::vector<FlexLine> lines;
    collectFlexLines(node, containerMain, isRow, isWrap, gap, lines);

    float currentCrossPos = 0;

    for (auto& line : lines) {
        float usedMain = line.mainSize;
        float freeMain = containerMain - usedMain;

        float startOffset = 0;
        float gapStep = 0;
        ldt::JustifyContent justify = prop->getJustifyContent();
        
        if (justify == ldt::JustifyContent::FlexEnd) startOffset = freeMain;
        else if (justify == ldt::JustifyContent::Center) startOffset = freeMain / 2.0f;
        else if (justify == ldt::JustifyContent::SpaceBetween) {
            if (line.childIndices.size() > 1) gapStep = freeMain / (line.childIndices.size() - 1);
        }
        else if (justify == ldt::JustifyContent::SpaceAround) {
            if (!line.childIndices.empty()) {
                gapStep = freeMain / line.childIndices.size();
                startOffset = gapStep / 2.0f;
            }
        }
        else if (justify == ldt::JustifyContent::SpaceEvenly) {
             if (!line.childIndices.empty()) {
                gapStep = freeMain / (line.childIndices.size() + 1);
                startOffset = gapStep;
            }
        }

        float currentMainPos = startOffset;
        ldt::AlignItems alignItems = prop->getAlignItems();

        // line cross（与 resolve 一致）：单行 = 容器交叉尺寸
        float lineCrossSize = (lines.size() == 1) ? containerCross : line.crossSize;

        for (size_t i = 0; i < line.childIndices.size(); ++i) {
            size_t idx = line.childIndices[i];
            auto& child = node->getFlowChildren()[idx];
            float childMain = getMainSize(child, isRow);
            float childCross = getCrossSize(child, isRow);
            
            float alignOffset = 0;
            
            if (alignItems == ldt::AlignItems::Center) {
                alignOffset = (lineCrossSize - childCross) / 2.0f;
            } else if (alignItems == ldt::AlignItems::FlexEnd) {
                alignOffset = lineCrossSize - childCross;
            }
            
            float childX = 0;
            float childY = 0;
            
            if (isRow) {
                float mainPos = currentMainPos;
                if (reverse) {
                    mainPos = containerMain - currentMainPos - childMain;
                }
                childX = contentAbsoluteX + mainPos;
                childY = contentAbsoluteY + currentCrossPos + alignOffset;
            } else {
                float mainPos = currentMainPos;
                if (reverse) {
                    mainPos = containerMain - currentMainPos - childMain;
                }
                childX = contentAbsoluteX + currentCrossPos + alignOffset;
                childY = contentAbsoluteY + mainPos;
            }
            
            engine->layoutPhase(child, childX, childY);
            
            currentMainPos += childMain + gap + gapStep;
            if (justify == ldt::JustifyContent::SpaceAround) currentMainPos += gapStep;
        }
        
        currentCrossPos += line.crossSize + gap;
    }
}
