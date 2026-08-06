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

    if (requestedW != ldt::AUTO_SENTINEL) {
        contentW = requestedW;
    } else {
        bool isInline = (prop->getDisplay() == ldt::FormattingContext::Inline);
        // A block-level flex container with auto width should use the finite
        // width offered by its parent. Overflowing children are represented by
        // scrollWidth, not by expanding the container's computed width.
        if (!isInline && isfinite(availableForChildrenW) && availableForChildrenW >= 0.0f) {
            contentW = availableForChildrenW;
        }
    }

    
    if (requestedH != ldt::AUTO_SENTINEL) contentH = requestedH;

    node->layout.computedWidth = clampf_flex(contentW, node->layout.minWidth, node->layout.maxWidth);
    node->layout.computedHeight = clampf_flex(contentH, node->layout.minHeight, node->layout.maxHeight);
}

void FlexLayout::layoutFlex(BoxModelEngine* engine, ldt::ResolvedNode* node,
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
                    engine->reMeasureChildren(child);
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
                    engine->reMeasureChildren(child);
                    reducedSpaceTotal += reduced;
                }
                if (reducedSpaceTotal <= 0.0f) break;
                refreshLine();
            }
        }
        
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


        float lineCrossSize = line.crossSize;
        if (lines.size() == 1 && containerCross > lineCrossSize) {
            lineCrossSize = containerCross;
        }

        for (size_t i = 0; i < line.childIndices.size(); ++i) {
            size_t idx = line.childIndices[i];
            auto& child = node->getFlowChildren()[idx];
            float childMain = getMainSize(child, isRow);
            float childCross = getCrossSize(child, isRow);
            
            float alignOffset = 0;
            
            if (alignItems == ldt::AlignItems::Stretch) {
                const IPropertyProvider& childRes = child->props();
                float targetCross = lineCrossSize;
                if (lines.size() == 1 && childRes.getOverflow() != ldt::Overflow::Visible) {
                    targetCross = containerCross;
                }
                    if (isRow) {
                    if (childRes.getDisplay() != ldt::FormattingContext::Inline && childRes.getHeight().isAuto()) {
                         float availableForContent = targetCross - child->layout.margin.vertical() - child->layout.border.vertical() - child->layout.padding.vertical();
                         child->layout.computedHeight = clampf_flex(
                             std::max(0.0f, availableForContent),
                             child->layout.minHeight,
                             child->layout.maxHeight);
                         child->layout.finalizeSizes();
                         childCross = child->layout.getMarginBox().height;
                         // Re-measure children of this item because its size changed (stretch)
                         engine->reMeasureChildren(child);
                    }
                } else {
                    if (childRes.getDisplay() != ldt::FormattingContext::Inline && childRes.getWidth().isAuto()) {
                        float availableForContent = targetCross - child->layout.margin.horizontal() - child->layout.border.horizontal() - child->layout.padding.horizontal();
                        child->layout.computedWidth = clampf_flex(
                            std::max(0.0f, availableForContent),
                            child->layout.minWidth,
                            child->layout.maxWidth);
                        child->layout.finalizeSizes();
                        childCross = child->layout.getMarginBox().width;
                        // Re-measure children of this item because its size changed (stretch)
                        engine->reMeasureChildren(child);
                    }
                }
            } else if (alignItems == ldt::AlignItems::Center) {


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
