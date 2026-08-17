#include "block_layout.h"
#include "engine/box_model_engine.h"
#include "engine/document_runtime.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <render/drawer.h>
#include "misc/float_utils.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_node.h"
#include "engine/core/ast_node.h"
#include "engine/core/property_resolver.h"

using namespace std;
using namespace ldt;



static float clampf_block(float v, float lo, float hi) {
    if (!isnan(lo) && v < lo) v = lo;
    if (!isnan(hi) && v > hi) v = hi;
    return v;
}

void BlockLayout::measureBlock(BoxModelEngine* engine, ldt::ResolvedNode* node,
                               float availableForChildrenW, float availableForChildrenH,
                               float requestedW, float requestedH) {
    if (!engine || !node) return;
    auto& cl = node->layout;

    // Measure children with width constraint = availableForChildrenW (if bounded)
    // 混合排列：block 子元素独占一行（垂直堆叠）；inline 子元素形成行内流（水平并排，超宽换行）
    float maxW = 0;
    float totalH = 0;
    float curLineW = 0.0f;
    float lineH = 0.0f;

    auto flushLine = [&]() {
        if (curLineW > maxW) maxW = curLineW;
        if (lineH > 0.0f) totalH += lineH;
        curLineW = 0.0f;
        lineH = 0.0f;
    };

    if (!node->getFlowChildren().empty()) {
        for (auto* ch : node->getFlowChildren()) {
            if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
            engine->measurePhase(ch, availableForChildrenW, availableForChildrenH, node->props().getDisplay(), requestedW != ldt::AUTO_SENTINEL, requestedH != ldt::AUTO_SENTINEL);
            float itemW = ch->layout.getMarginBox().width;
            float itemH = ch->layout.getMarginBox().height;

            if (ch->props().getDisplay() == ldt::FormattingContext::Inline) {
                // inline 子元素：加入当前行，超宽换行
                if (isfinite(availableForChildrenW) && ldt::floatGreater(curLineW + itemW, availableForChildrenW) && curLineW > 0.0f) {
                    flushLine();
                }
                curLineW += itemW;
                lineH = std::max(lineH, itemH);
            } else {
                // block 子元素：先结束当前 inline 行，再独占一行
                flushLine();
                maxW = std::max(maxW, itemW);
                totalH += itemH;
            }
        }
    }
    flushLine();

    float contentW = (requestedW == ldt::AUTO_SENTINEL) ? maxW : requestedW;
    float contentH = (requestedH == ldt::AUTO_SENTINEL) ? totalH : requestedH;

    // 内在（内容）尺寸：与最终尺寸分离，供 resolve 阶段 / 调试使用
    cl.intrinsicWidth = contentW;
    cl.intrinsicHeight = contentH;

    cl.computedWidth = clampf_block(contentW, cl.minWidth, cl.maxWidth);
    cl.computedHeight = clampf_block(contentH, cl.minHeight, cl.maxHeight);
}

void BlockLayout::layoutBlock(BoxModelEngine* engine, ldt::ResolvedNode* node,
                              float contentAbsoluteX, float contentAbsoluteY) {
    if (!engine || !node) return;
    auto& cl = node->layout;

    // 混合排列：block 子元素独占一行（垂直堆叠）；inline 子元素水平并排（超宽换行）
    float containerW = cl.getContentBox().width;
    float curX = contentAbsoluteX;
    float curY = contentAbsoluteY;
    float curLineH = 0.0f;

    for (auto* ch : node->getFlowChildren()) {
        if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
        float itemW = ch->layout.getMarginBox().width;
        float itemH = ch->layout.getMarginBox().height;

        if (ch->props().getDisplay() == ldt::FormattingContext::Inline) {
            // inline 子元素：水平并排，超宽换行
            if (isfinite(containerW) && ldt::floatGreater(curX + itemW - contentAbsoluteX, containerW) && curX > contentAbsoluteX) {
                curX = contentAbsoluteX;
                curY += curLineH;
                curLineH = 0.0f;
            }
            engine->layoutPhase(ch, curX, curY);
            curX += itemW;
            curLineH = std::max(curLineH, itemH);
        } else {
            // block 子元素：先结束当前 inline 行，再独占一行
            curX = contentAbsoluteX;
            curY += curLineH;
            curLineH = 0.0f;
            engine->layoutPhase(ch, contentAbsoluteX, curY);
            curY += itemH;
        }
    }
}


