#include "flex_layout.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <string>
#include <cctype>
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

static float getFlexAutoMinMainContentSize(const ldt::ResolvedNode* node,
                                           const IPropertyProvider& props,
                                           bool isRow) {
    if (isRow) {
        return node->layout.minWidth;
    }

    float minMain = node->layout.minHeight;
    if (props.getHeight().isAuto()) {
        minMain = std::max(minMain, node->layout.computedHeight);
    }
    return minMain;
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
        
        float totalFlexGrow = 0;
        for (size_t idx : line.childIndices) {
            totalFlexGrow += node->getFlowChildren()[idx]->props().getFlexGrow();
        }
        if (totalFlexGrow > 0 && freeMain > 0) {
            float addedSpaceTotal = 0;
            for (size_t idx : line.childIndices) {
                auto& child = node->getFlowChildren()[idx];
                const IPropertyProvider& childRes = child->props();
                float grow = childRes.getFlexGrow();
                if (grow > 0) {
                    float share = (grow / totalFlexGrow) * freeMain;
                    if (isRow) {
                        child->layout.computedWidth += share;
                        child->layout.finalizeSizes();
                    } else {
                        child->layout.computedHeight += share;
                        child->layout.finalizeSizes();
                    }

                    // Re-measure children of this item because its size changed
                    engine->reMeasureChildren(child);
                    addedSpaceTotal += share;
                }
            }
            line.mainSize += addedSpaceTotal;
            usedMain += addedSpaceTotal;
            freeMain -= addedSpaceTotal;
        }

        if (freeMain < 0.0f) {
            float totalScaledShrink = 0.0f;
            for (size_t idx : line.childIndices) {
                auto& child = node->getFlowChildren()[idx];
                const IPropertyProvider& childRes = child->props();
                const float shrink = std::max(0.0f, childRes.getFlexShrink());
                if (shrink <= 0.0f) {
                    continue;
                }
                totalScaledShrink += getMainSize(child, isRow) * shrink;
            }

            if (totalScaledShrink > 0.0f) {
                const float overflowMain = -freeMain;
                for (size_t idx : line.childIndices) {
                    auto& child = node->getFlowChildren()[idx];
                    const IPropertyProvider& childRes = child->props();
                    const float shrink = std::max(0.0f, childRes.getFlexShrink());
                    if (shrink <= 0.0f) {
                        continue;
                    }

                    const float basisMain = getMainSize(child, isRow);
                    if (basisMain <= 0.0f) {
                        continue;
                    }

                    const float reduction = overflowMain * ((basisMain * shrink) / totalScaledShrink);
                    const float minMainContent = getFlexAutoMinMainContentSize(child, childRes, isRow);

                    bool changed = false;
                    if (isRow) {
                        const float nextWidth = std::max(minMainContent, child->layout.computedWidth - reduction);
                        changed = !ldt::floatEqual(nextWidth, child->layout.computedWidth);
                        child->layout.computedWidth = nextWidth;
                    } else {
                        const float nextHeight = std::max(minMainContent, child->layout.computedHeight - reduction);
                        changed = !ldt::floatEqual(nextHeight, child->layout.computedHeight);
                        child->layout.computedHeight = nextHeight;
                    }

                    if (changed) {
                        child->layout.finalizeSizes();
                        engine->reMeasureChildren(child);
                    }
                }

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
                float targetCross = lineCrossSize; 
                const IPropertyProvider& childRes = child->props();
                    if (isRow) {
                    if (childRes.getDisplay() != ldt::FormattingContext::Inline && childRes.getHeight().isAuto()) {
                         float availableForContent = targetCross - child->layout.margin.vertical() - child->layout.border.vertical() - child->layout.padding.vertical();
                         child->layout.computedHeight = std::max(0.0f, availableForContent);
                         child->layout.finalizeSizes();
                         childCross = child->layout.getMarginBox().height;
                         // Re-measure children of this item because its size changed (stretch)
                         engine->reMeasureChildren(child);
                    }
                } else {
                    if (childRes.getDisplay() != ldt::FormattingContext::Inline && childRes.getWidth().isAuto()) {
                        float availableForContent = targetCross - child->layout.margin.horizontal() - child->layout.border.horizontal() - child->layout.padding.horizontal();
                        child->layout.computedWidth = std::max(0.0f, availableForContent);
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
