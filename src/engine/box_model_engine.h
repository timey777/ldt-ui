#pragma once

#include "engine.h"
#include <map>
#include <unordered_map>
#include <string>
#include "engine/core/node_flags.h"
#include "text_measure_cache.h"
namespace ldt { class ResolvedNode; }
// 盒模型计算引擎
// 职责：
// 1. 从节点属性读取 width/height/x/y/padding/margin/border
// 2. 从 StyleEngine 读取盒模型相关样式
// 3. 计算最终的 ComputedLayout (contentBox, paddingBox, borderBox, marginBox)
// 4. 不处理布局算法，只计算单个元素的盒模型
class BoxModelEngine : public AbstractEngine {
public:
    BoxModelEngine();
    ~BoxModelEngine();
    
    void initialize() override;
    void shutdown() override;
    
    std::type_index type() const override;
    void process(std::shared_ptr<ASTNode> root, ldt::DisplayList& outList, 
                 float viewportW, float viewportH) override;
    
    std::string name() const override;
    
    // 热重载支持：增量更新
    void onASTUpdated(std::shared_ptr<ASTNode> root) override;
    
    
    // 重置引擎状态
    void reset();

    // Measure & layout pass methods (converted from free functions)
    void measurePhase(ldt::ResolvedNode* node, float containerContentW, float containerContentH, ldt::FormattingContext parentCtx, bool parentWidthDefinite = true, bool parentHeightDefinite = true);
    // resolve 阶段：自顶向下，父级用已解析的自身尺寸定子级最终尺寸（flex grow/shrink/stretch），再递归。
    // 定位之前必须先跑 resolvePhase。
    void resolvePhase(ldt::ResolvedNode* node);
    // Re-measure children of a node that has been resized (e.g. by flex-grow/flex-shrink
    // or align-items:stretch).
    // widthDefinite/heightDefinite: true when the caller has authoritatively resolved that
    // axis. flex-grow/flex-shrink only resolve the main axis, so the cross axis is passed as
    // not definite; if the node has no explicit width/height for it, it is re-derived from the
    // re-measured content instead of being locked to the stale pre-resize value.
    void reMeasureChildren(ldt::ResolvedNode* node, bool widthDefinite, bool heightDefinite);

    // Re-calculates positions for node and its children based on current sizes and parent coordinates.
    // Skips measurement phase. Use this for things like dragging or simple position updates.
    void layoutSubtree(ldt::ResolvedNode* node);

    void layoutPhase(ldt::ResolvedNode* node, float parentContentX, float parentContentY);
private:
	// Handle absolute positioning logic for overlays (center/anchor + offsetX/Y)
	void applyAnchor(ldt::ResolvedNode& node, const ldt::ResolvedNode& parent);

    // Read margin/padding/border from StyleEngine / attributes
    void readBoxModelProperties(ldt::ResolvedNode* node);

    // Content measurement helpers
    void measureContent(ldt::ResolvedNode* node, float& requestedW, float& requestedH, float availableWidth);

    ldt::TextMeasureCache textMeasureCache_;

public:
    // Scroll state calculation (public for external recalculation, e.g. preview attach)
    void calculateScrollState(ldt::ResolvedNode* node);
};