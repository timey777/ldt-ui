#include "inline_layout.h"
#include "engine/box_model_engine.h"
#include "engine/document_runtime.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "engine/core/resolved_node.h"
#include "engine/core/property_resolver.h"
#include "misc/float_utils.h"
using namespace std;
using namespace ldt;



// Helper to clamp like other modules
static float clampf_inline(float v, float lo, float hi) {
    if (!isnan(lo) && v < lo) v = lo;
    if (!isnan(hi) && v > hi) v = hi;
    return v;
}

void InlineLayout::measureInline(BoxModelEngine* engine, ldt::ResolvedNode* node,
                                 float availableForChildrenW, float availableForChildrenH,
                                 float requestedW, float requestedH) {
    if (!engine || !node) return;
    const IPropertyProvider* prop = &node->props();
    auto& cl = node->layout;

    bool isInlineBlock = (prop->getDisplay() == ldt::FormattingContext::Inline);
    float containerW = (isfinite(availableForChildrenW) ? availableForChildrenW : ldt::UNBOUNDED);



    // Simple inline flow measurement: lay out children in lines, wrapping when exceeding containerW
    float curLineW = 0.0f;
    float maxLineW = 0.0f;
    float totalH = 0.0f;
    float lineH = 0.0f;

    auto pushLine = [&]() {
        if (curLineW > maxLineW) maxLineW = curLineW;
        if (lineH > 0.0f) totalH += lineH;
        curLineW = 0.0f;
        lineH = 0.0f;
    };

    // If node has no children but contains text, the engine may have set text into attributes.
    // We measure children first; if there are none, we'll try to handle node text in caller.
    if (node->getFlowChildren().empty()) {
        // Leaf node. If it had text, BoxModelEngine already measured it and set requestedW/H.
        // Just apply requested size overrides if not auto
        cl.computedWidth = (requestedW == ldt::AUTO_SENTINEL) ? 0.0f : requestedW;
        cl.computedHeight = (requestedH == ldt::AUTO_SENTINEL) ? 0.0f : requestedH;
        return;
    }

    for (auto* ch : node->getFlowChildren()) {
        const IPropertyProvider& childProp = ch->props();
        if (childProp.getDisplay() == ldt::FormattingContext::None) continue;
        // measure child with no width constraint to get intrinsic inline size
        engine->measurePhase(ch, ldt::UNBOUNDED, ldt::UNBOUNDED, prop->getDisplay(), false, false);
        float itemW = ch->layout.getMarginBox().width;
        float itemH = ch->layout.getMarginBox().height;



        if (isfinite(containerW) && ldt::floatGreater(curLineW + itemW, containerW) && curLineW > 0.0f) {
            // wrap
            pushLine();
        }
        curLineW += itemW;
        lineH = max(lineH, itemH);
    }

    if (curLineW > 0.0f || lineH > 0.0f) pushLine();

    float finalW = isInlineBlock ? maxLineW : (isfinite(containerW) ? min(maxLineW, containerW) : maxLineW);
    float finalH = totalH;

    cl.computedWidth = clampf_inline((requestedW == ldt::AUTO_SENTINEL) ? finalW : requestedW, cl.minWidth, cl.maxWidth);
    cl.computedHeight = clampf_inline((requestedH == ldt::AUTO_SENTINEL) ? finalH : requestedH, cl.minHeight, cl.maxHeight);
}

void InlineLayout::layoutInline(BoxModelEngine* engine, ldt::ResolvedNode* node,
                                float contentAbsoluteX, float contentAbsoluteY) {
    if (!engine || !node) return;
    const IPropertyProvider* prop = &node->props();
    auto& cl = node->layout;

    bool isInlineBlock = (prop->getDisplay() == ldt::FormattingContext::Inline);
    float containerW = cl.getContentBox().width; // contentBox should be set by measure pass

    float curX = contentAbsoluteX;
    float curY = contentAbsoluteY;
    float curLineH = 0.0f;

    for (size_t i = 0; i < node->getFlowChildren().size(); ++i) {
        auto* ch = node->getFlowChildren()[i];
        const IPropertyProvider& childProp = ch->props();
        if (childProp.getDisplay() == ldt::FormattingContext::None) continue;
        float itemW = ch->layout.getMarginBox().width;
        float itemH = ch->layout.getMarginBox().height;


        // wrap when necessary (for inline, if a container width exists)
        if (isfinite(containerW) && ldt::floatGreater(curX + itemW - contentAbsoluteX, containerW) && curX > contentAbsoluteX) {
            // move to next line
            curX = contentAbsoluteX;
            curY += curLineH;
            curLineH = 0.0f;
        }

        // place child: parentForChildX/Y expect parent's content origin
        float parentForChildX = curX;
        float parentForChildY = curY;
        engine->layoutPhase(ch, parentForChildX, parentForChildY);

        curX += itemW;
        curLineH = max(curLineH, itemH);
    }
}

