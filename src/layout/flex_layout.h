// Encapsulate flex measurement and layout logic for BoxModelEngine
#ifndef FLEX_LAYOUT_H
#define FLEX_LAYOUT_H

#include "engine/box_model_engine.h"
#include <memory>
#include <vector>
#include <string>
#include <engine/core/resolved_tree.h>
namespace ldt {
    class ResolvedNode;
}
class FlexLayout {
public:
    static void measureFlex(BoxModelEngine* engine, ldt::ResolvedNode* node,
                             float availableForChildrenW, float availableForChildrenH,
                             float requestedW, float requestedH);

    static void layoutFlex(BoxModelEngine* engine, ldt::ResolvedNode* node,
                           float contentAbsoluteX, float contentAbsoluteY);

private:
    struct FlexLine {
        std::vector<size_t> childIndices;
        float mainSize = 0.0f;
        float crossSize = 0.0f;
    };

    static bool isRowDirection(ldt::FlexDirection flexDirection);
    static bool isReverseDirection(ldt::FlexDirection flexDirection);
    
    static float getMainSize(const ldt::ResolvedNode* node, bool isRow);
    static float getCrossSize(const ldt::ResolvedNode* node, bool isRow);
    static float getMarginMain(const ldt::ResolvedNode* node, bool isRow);
    static float getMarginCross(const ldt::ResolvedNode* node, bool isRow);
    
    static void collectFlexLines(ldt::ResolvedNode* node,
                                 float availableMain, 
                                 bool isRow, 
                                 bool isWrap, 
                                 float gap,
                                 std::vector<FlexLine>& outLines);
};

#endif // FLEX_LAYOUT_H
