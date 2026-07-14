#include "block_layout.h"
#include "engine/box_model_engine.h"
#include "engine/document_runtime.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <render/drawer.h>
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

// Extract text for semantic nodes (same whitelist used elsewhere)
static std::string extractNodeTextLocal(ldt::ResolvedNode* rn) {
    if (!rn || !rn->astNode) return "";
    std::string ttype = rn->astNode->type;
    if (ttype == "text" || ttype == "label" || ttype == "button" || ttype == "input" || ttype == "textarea") {
        if (auto a = rn->astNode->getAttribute("text")) { if (a->isString()) return a->as<std::string>(); }
        if (auto a = rn->astNode->getAttribute("title")) { if (a->isString()) return a->as<std::string>(); }
        if (auto a = rn->astNode->getAttribute("value")) { if (a->isString()) return a->as<std::string>(); }
        if (auto a = rn->astNode->getAttribute("placeholder")) { if (a->isString()) return a->as<std::string>(); }
    }
    return "";
}

void BlockLayout::measureBlock(BoxModelEngine* engine, ldt::ResolvedNode* node,
                               float availableForChildrenW, float availableForChildrenH,
                               float requestedW, float requestedH) {
    if (!engine || !node) return;
    auto& cl = node->layout;

    // Measure children with width constraint = availableForChildrenW (if bounded)
    float maxW = 0;
    float totalH = 0;
    if (!node->getFlowChildren().empty()) {
        for (auto* ch : node->getFlowChildren()) {
            if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
            engine->measurePhase(ch, availableForChildrenW, availableForChildrenH, node->props().getDisplay(), requestedW != ldt::AUTO_SENTINEL, requestedH != ldt::AUTO_SENTINEL);
            maxW = max(maxW, ch->layout.getMarginBox().width);
            totalH += ch->layout.getMarginBox().height;
        }
    }


    // else: leaf node. If it had text, BoxModelEngine already measured it and set requestedW/H (if they were auto).
    // So we just use the requested sizes below.

    float contentW = (requestedW == ldt::AUTO_SENTINEL) ? maxW : requestedW;
    float contentH = (requestedH == ldt::AUTO_SENTINEL) ? totalH : requestedH;

    cl.computedWidth = clampf_block(contentW, cl.minWidth, cl.maxWidth);
    cl.computedHeight = clampf_block(contentH, cl.minHeight, cl.maxHeight);
}

void BlockLayout::layoutBlock(BoxModelEngine* engine, ldt::ResolvedNode* node,
                              float contentAbsoluteX, float contentAbsoluteY) {
    if (!engine || !node) return;
    auto& cl = node->layout;

    float curY = contentAbsoluteY;
    for (auto* ch : node->getFlowChildren()) {
        if (ch->props().getDisplay() == ldt::FormattingContext::None) continue;
        float parentForChildX = contentAbsoluteX;
        float parentForChildY = curY;
        engine->layoutPhase(ch, parentForChildX, parentForChildY);
        curY += ch->layout.getMarginBox().height;
    }
}


