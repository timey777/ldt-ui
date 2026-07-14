#pragma once

#include <memory>
namespace ldt { class ResolvedNode; }
class BoxModelEngine;

class BlockLayout {
public:
    // measure for block content (vertical stacking, text wrapping for leaf text)
        static void measureBlock(BoxModelEngine* engine, ldt::ResolvedNode* node,
                             float availableForChildrenW, float availableForChildrenH,
                             float requestedW, float requestedH);

    // layout for block content (vertical stacking)
        static void layoutBlock(BoxModelEngine* engine, ldt::ResolvedNode* node,
                            float contentAbsoluteX, float contentAbsoluteY);
};
