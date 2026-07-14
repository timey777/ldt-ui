#pragma once

#include <memory>
namespace ldt { class ResolvedNode; }
class BoxModelEngine;

class InlineLayout {
public:
    // measure for inline/inline-block content
    static void measureInline(BoxModelEngine* engine, ldt::ResolvedNode* node,
                              float availableForChildrenW, float availableForChildrenH,
                              float requestedW, float requestedH);

    // layout for inline/inline-block content
    static void layoutInline(BoxModelEngine* engine, ldt::ResolvedNode* node,
                             float contentAbsoluteX, float contentAbsoluteY);
};
