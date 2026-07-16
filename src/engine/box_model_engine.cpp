#include "box_model_engine.h"
#include "document_runtime.h"
#include "layout/flex_layout.h"
#include "layout/inline_layout.h"
#include "layout/block_layout.h"
#include "misc/image_util.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include "render/text_measurer.h"
#include <iostream>
#include "misc/logger.h"
#include "misc/float_utils.h"
#include "misc/perf_timer.h"
#include "components/abstract_control.h"
#include "components/scrollbar_control.h"
#include <components/container_control.h>
#include "engine/core/ast_node.h"
#include "engine/core/resolved_node.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/property_resolver.h"

using namespace std;
using namespace ldt;

std::string BoxModelEngine::name() const { return "BoxModelEngine"; }

BoxModelEngine::BoxModelEngine() = default;
BoxModelEngine::~BoxModelEngine() = default;

void BoxModelEngine::initialize() {}
void BoxModelEngine::shutdown() {}
void BoxModelEngine::reset() {}

// Read margin/padding/border from finalStyle (string) and populate layout fields
void BoxModelEngine::readBoxModelProperties(ldt::ResolvedNode* node) {
	if (!node || !node->astNode) return;
	if (getContext()) {
		// Use IPropertyProvider to read box model data
		const IPropertyProvider* prop = &node->props();
		auto& layout = node->layout;

		
		layout.margin = prop->getMargin();
		std::array<bool, 4> mAuto = prop->getMarginAuto();
		layout.marginTopAuto = mAuto[0];
		layout.marginRightAuto = mAuto[1];
		layout.marginBottomAuto = mAuto[2];
		layout.marginLeftAuto = mAuto[3];

		layout.padding = prop->getPadding();
		layout.border = prop->getBorderWidth();

		layout.wrap = prop->getDisplay() != ldt::FormattingContext::Inline;
	}
}



// ----------------- Helpers -----------------

// parse size strings: "auto", "100px", "50%" or plain number
// parse size strings: "auto", "100px", "50%" or plain number
// returns AUTO_SENTINEL for "auto", otherwise pixel value.
// For percent, parentSize must be passed.
float parseSize(const ldt::LayoutUnit& u, float parentSize, bool parentIsDefinite = true) {
	return u.resolve(parentSize, parentIsDefinite);
}

// clamp helper
float clampf(float v, float lo, float hi) {
	if (!isnan(lo) && v < lo) v = lo;
	if (!isnan(hi) && v > hi) v = hi;
	return v;
}

void BoxModelEngine::measureContent(ldt::ResolvedNode* node, float& requestedW, float& requestedH, float availableWidth) {
	const IPropertyProvider* prop = &node->props();
	if (node->astNode->type == "image") {
		int imgW = 0, imgH = 0;
		std::string src;
		if (auto attr = node->astNode->getAttribute("src")) {
			src = attr->as<std::string>();
		}
		if (!src.empty()) {
			if (ldt::ImageUtil::getImageSize(src, imgW, imgH)) {
				if (requestedW == ldt::AUTO_SENTINEL && imgW > 0) requestedW = (float)imgW;
				if (requestedH == ldt::AUTO_SENTINEL && imgH > 0) requestedH = (float)imgH;
			}
		}
	}
	else if (node->astNode->type == "text") {
		ITextMeasurer* textMeasureer = nullptr;
		if (getContext()) textMeasureer = getContext()->getTextMeasurer();
		if (!textMeasureer) return;

		std::string text;
		if (auto attr = node->astNode->getAttribute("value")) {
			text = attr->as<std::string>();
		}
		if (text.empty()) return;

		Font font;
		font.family = prop->getFontFamily();
		font.size = prop->getFontSize();
		font.bold = (prop->getFontWeight() == ldt::FontWeight::Bold || prop->getFontWeight() == ldt::FontWeight::W700);

		float wrapW = -1.0f;
		if (node->layout.wrap) {
			if (requestedW != ldt::AUTO_SENTINEL) wrapW = requestedW;
			else if (isfinite(availableWidth)) wrapW = availableWidth;
		}

		auto cached = textMeasureCache_.get(text, font.family, font.size, font.bold, wrapW, textMeasureer);
		float textW = cached.intrinsicWidth;
		float textH = cached.lineHeight > 0.0f ? cached.lineHeight : (cached.ascent + cached.descent);

		// Apply line-height if specified
		if (prop->getLineHeight() > 0.0f) {
			FontDesc fdesc{ font.family, font.size };
			float actualSingleLH = textMeasureer->getLineHeight(fdesc);
			if (actualSingleLH > 0.1f) {
				float lines = textH / actualSingleLH;
				textH = std::round(lines) * prop->getLineHeight();
			}
		}

		if (requestedW == ldt::AUTO_SENTINEL) {
			bool isBlockLevel = (prop->getDisplay() == ldt::FormattingContext::Block || prop->getDisplay() == ldt::FormattingContext::Flex || prop->getDisplay() == ldt::FormattingContext::Grid);
			if (isBlockLevel && isfinite(availableWidth)) {
				requestedW = availableWidth;
			}
			else {
				if (wrapW > 0 && ldt::floatGreater(textW, wrapW)) {
					requestedW = wrapW;
				}
				else {
					requestedW = textW;
				}
			}
		}

		if (requestedH == ldt::AUTO_SENTINEL) {
			requestedH = textH;
		}
	}
	else if (node->astNode->type == "input") {
		// Inputs: support single-line (default) and multiline (textarea) behaviors.
		ITextMeasurer* textMeasureer = nullptr;
		if (getContext()) textMeasureer = getContext()->getTextMeasurer();
		// determine input text: prefer value, fall back to placeholder
		std::string text;
		if (auto attr = node->astNode->getAttribute("value")) text = attr->as<std::string>();
		if (text.empty()) {
			if (auto attr = node->astNode->getAttribute("placeholder")) text = attr->as<std::string>();
		}
		// get input type attribute ("textarea" => multiline)
		std::string inputType;
		if (auto attr = node->astNode->getAttribute("type")) inputType = attr->as<std::string>();
		bool isMultiline = (inputType == "textarea");

		// Font
		Font font;
		font.family = prop->getFontFamily();
		font.size = prop->getFontSize() > 0.0f ? prop->getFontSize() : 14.0f;
		font.bold = (prop->getFontWeight() == ldt::FontWeight::Bold || prop->getFontWeight() == ldt::FontWeight::W700);

		// Ensure we always have a baseline height even when text is empty and height is auto
		float defaultLineH = font.size * 1.2f;

		float textW = 0.0f;
		if (textMeasureer && !text.empty()) {
			FontDesc fdesc; fdesc.family = font.family; fdesc.size = font.size;
			TextMetrics tmw = textMeasureer->measureText(text, fdesc, -1.0f);
			textW = tmw.width;
		}

		if (isMultiline) {
			float wrapW = -1.0f;
			if (node->layout.wrap) {
				if (requestedW != ldt::AUTO_SENTINEL) wrapW = requestedW;
				else if (isfinite(availableWidth)) wrapW = availableWidth;
			}
			float textH = defaultLineH;
			if (textMeasureer) {
				FontDesc fdesc; fdesc.family = font.family; fdesc.size = font.size;
				TextMetrics tmh = textMeasureer->measureText(text.empty() ? std::string(" ") : text, fdesc, wrapW);
				textH = tmh.lineHeight > 0.0f ? tmh.lineHeight : (tmh.ascent + tmh.descent);
			}
			if (prop->getLineHeight() > 0.0f) {
				float lines = textH / (font.size * 1.2f);
				textH = lines * prop->getLineHeight();
			}

			if (requestedW == ldt::AUTO_SENTINEL) {
				bool isBlockLevel = (prop->getDisplay() == ldt::FormattingContext::Block || prop->getDisplay() == ldt::FormattingContext::Flex || prop->getDisplay() == ldt::FormattingContext::Grid);
				if (isBlockLevel && isfinite(availableWidth)) requestedW = availableWidth;
				else {
					if (wrapW > 0 && ldt::floatGreater(textW, wrapW)) requestedW = wrapW;
					else requestedW = textW;
				}
			}

			if (requestedH == ldt::AUTO_SENTINEL) {
				// At minimum, one line height
				requestedH = std::max(defaultLineH, textH);
			}
		} else {
			// single-line input: do not wrap text; height is single-line height when auto
			if (requestedW == ldt::AUTO_SENTINEL) {
				bool isBlockLevel = (prop->getDisplay() == ldt::FormattingContext::Block || prop->getDisplay() == ldt::FormattingContext::Flex || prop->getDisplay() == ldt::FormattingContext::Grid);
				if (isBlockLevel && isfinite(availableWidth)) {
					requestedW = availableWidth;
				} else {
					if (textMeasureer) requestedW = textW;
					else requestedW = 0.0f;
				}
			}
			if (requestedH == ldt::AUTO_SENTINEL) {
				requestedH = defaultLineH;
			}
		}
	}
}

static void applyBoxSizing(
	ldt::ResolvedNode* node,
	float& contentW,
	float& contentH
) {
	const IPropertyProvider* prop = &node->props();
	if (prop->getBoxSizing() != ldt::BoxSizing::BorderBox)
		return;

	const auto& bw = prop->getBorderWidth();
	const auto& l = node->layout;

	if (isDefinite(contentW)) {
		contentW -= (l.padding.horizontal() + bw.horizontal());
		contentW = std::max(0.0f, contentW);
	}

	if (isDefinite(contentH)) {
		contentH -= (l.padding.vertical() + bw.vertical());
		contentH = std::max(0.0f, contentH);
	}
}
static float computeAvailable(ldt::ResolvedNode* node, float requested, float parentContent, bool isWidth)
{
	float avail = (requested != ldt::AUTO_SENTINEL) ? requested : (isfinite(parentContent) ? parentContent : ldt::UNBOUNDED);
	const IPropertyProvider* prop = &node->props();

	if (requested == ldt::AUTO_SENTINEL && isfinite(avail)) {

		const auto& bw = prop->getBorderWidth();
		if (isWidth) {
			float horizontalExtras = prop->getPadding().horizontal() + bw.horizontal() + prop->getMargin().horizontal();
			avail = std::max(0.0f, avail - horizontalExtras);
		} else {
			float verticalExtras = prop->getPadding().vertical() + bw.vertical() + prop->getMargin().vertical();
			avail = std::max(0.0f, avail - verticalExtras);
		}
	}

	return avail;
}
static ldt::FormattingContext resolveFormattingContext(ldt::ResolvedNode* node, ldt::FormattingContext parentCtx)
{
	ldt::FormattingContext myCtx = node->props().getDisplay();
	return myCtx;
}
static void finalizeBoxes(ldt::ResolvedNode* node) {
	node->layout.finalizeSizes();
}
void BoxModelEngine::measurePhase(ldt::ResolvedNode* node, float parentContentWidth, float parentContentHeight, ldt::FormattingContext parentCtx, bool parentWidthDefinite, bool parentHeightDefinite) {

	readBoxModelProperties(node);
	// short-circuit invisible / display:none
	const IPropertyProvider* prop = &node->props();
	if (prop->getDisplay() == ldt:: FormattingContext::None) {

		node->layout.computedWidth = 0;
		node->layout.computedHeight = 0;
		node->layout.finalizeSizes();
		return;
	}
	
	// resolve provided width/height strings into requested sizes
	float requestedW = parseSize(prop->getWidth(), (isfinite(parentContentWidth) ? parentContentWidth : 0.0f), parentWidthDefinite);
	float requestedH = parseSize(prop->getHeight(), (isfinite(parentContentHeight) ? parentContentHeight : 0.0f), parentHeightDefinite);

	// Handle box-sizing: border-box
	applyBoxSizing(node, requestedW, requestedH);

	// Determine if *this* node has definite size for children
	bool childWidthDefinite = (requestedW != ldt::AUTO_SENTINEL);
	bool childHeightDefinite = (requestedH != ldt::AUTO_SENTINEL);

	// compute available space forwarded to children (availW/availH)
	float availW = computeAvailable(node, requestedW, parentContentWidth, true);
	float availH = computeAvailable(node, requestedH, parentContentHeight, false);

	// Special handling for content (image/text) auto size
	measureContent(node, requestedW, requestedH, availW);
	node->layout.computedWidth = isAuto(requestedW) ? 0 : requestedW;
	node->layout.computedHeight = isAuto(requestedH) ? 0 : requestedH;

	ldt::FormattingContext myCtx = resolveFormattingContext(node, parentCtx);
	if (myCtx == ldt::FormattingContext::Flex) {
		FlexLayout::measureFlex(this, node, availW, availH, requestedW, requestedH);
	}
	else if (myCtx == ldt::FormattingContext::Inline) {
		InlineLayout::measureInline(this, node, availW, availH, requestedW, requestedH);
	}
	else {
		BlockLayout::measureBlock(this, node, availW, availH, requestedW, requestedH);
	}
	finalizeBoxes(node);

	// Fill local boxes relative to marginBox origin (we'll set marginBox.x/y in layoutPhase)
	// Use non-negative offsets: borderBox starts at 0, paddingBox and contentBox are offset inward.

	//// marginBox size (width/height) - x,y set in layoutPhase
	//// During measure pass, treat 'auto' margins (NaN) as 0 for size calculations
	//float measMarginLeft = cl.marginLeftAuto ? 0.0f : cl.marginLeft;
	//float measMarginRight = cl.marginRightAuto ? 0.0f : cl.marginRight;
	//float measMarginTop = cl.marginTopAuto ? 0.0f : cl.marginTop;
	//float measMarginBottom = cl.marginBottomAuto ? 0.0f : cl.marginBottom;
	//cl.getMarginBox().width = cl.getBorderBox().width + measMarginLeft + measMarginRight;
	//cl.getMarginBox().height = cl.getBorderBox().height + measMarginTop + measMarginBottom;

	// NOTE: we intentionally do NOT modify child sizes for flex-grow here.
	// flex-grow will be handled in layoutPhase where we know the exact available space.

}

void BoxModelEngine::layoutSubtree(ldt::ResolvedNode* node) {
	if (!node) return;
	float pX = 0, pY = 0;
	if (node->parent) {
		const auto parentContentBox = node->parent->layout.getAbsoluteContentBox();
		pX = parentContentBox.x;
		pY = parentContentBox.y;
	}
	layoutPhase(node, pX, pY);
}

void BoxModelEngine::applyAnchor(ldt::ResolvedNode& node, const ldt::ResolvedNode& parent) {
	ldt::PropertyResolver resolver(&node);
	const IPropertyProvider* prop = &resolver;
	if (!prop->isOverlay()) return;


	float x = 0, y = 0;

	switch (prop->getAnchor()) {
	case ldt::Anchor::Center:
		x = (parent.layout.computedWidth - node.layout.computedWidth) * 0.5f;
		y = (parent.layout.computedHeight - node.layout.computedHeight) * 0.5f;
		break;
	case ldt::Anchor::TopLeft:
		x = 0;
		y = 0;
		break;
		// 其他 anchor 同理
	}
	node.layout.setPosition(x + prop->getOffsetX(), y + prop->getOffsetY());
}

// Layout pass: set absolute positions according to computed sizes. Similar to sample flex algorithm.
void BoxModelEngine::layoutPhase(ldt::ResolvedNode* node, float parentContentX, float parentContentY) {
	if (!node) return;
	// compute marginBox absolute position (marginBox.x/y are absolute world coords)
	// Treat NaN (auto) margins as 0 when computing absolute position to avoid NaN propagation.
	const IPropertyProvider* prop = &node->props();
	if (prop->isOverlay())

	{
		node->layout.setPosition(0, 0);
		this->applyAnchor(*node, *node->parent);
	}
	else {
		node->layout.setPosition(parentContentX, parentContentY);
	}

	// absolute positions for borderBox are already set correctly by setPosition
	// the following part can be removed if we are sure it is already covered.

	// however, if there's any absolute positioning logic that skips marginBox...
	// let's stick to setPosition and remove redundant manual sets.

	// paddingBox and contentBox remain relative (offsets inside borderBox)
	// compute absolute content origin from borderBox origin + contentBox local offset
	const auto contentAbsoluteBox = node->layout.getAbsoluteContentBox();
	float contentAbsoluteX = contentAbsoluteBox.x;
	float contentAbsoluteY = contentAbsoluteBox.y;

	// Now position children inside content area
	ldt::FormattingContext display = prop->getDisplay();
	bool isFlex = (display == ldt::FormattingContext::Flex);
	bool isInline = (display == ldt::FormattingContext::Inline);

	if (isFlex) {
		FlexLayout::layoutFlex(this, node, contentAbsoluteX, contentAbsoluteY);
	}
	else if (isInline) {
		InlineLayout::layoutInline(this, node, contentAbsoluteX, contentAbsoluteY);
	}
	else {

		// Block layout: resolve margin:auto for each child then delegate to BlockLayout
		float containerContentW = node->layout.getContentBox().width;
		for (auto& ch : node->getFlowChildren()) {
			const IPropertyProvider* childProp = &ch->props();
			if (childProp->getDisplay() == ldt::FormattingContext::None) continue;
			
			std::array<bool, 4> mAuto = childProp->getMarginAuto();
			bool leftAuto = mAuto[3];
			bool rightAuto = mAuto[1];

			if (leftAuto || rightAuto) {
				float childBorderW = ch->layout.getBorderBox().width;
				float remaining = containerContentW - childBorderW;
				if (remaining < 0.0f) remaining = 0.0f;
				
				ui::Edges childMargin = childProp->getMargin();
				float resolvedLeft = childMargin.left;
				float resolvedRight = childMargin.right;
				if (leftAuto && rightAuto) {
					resolvedLeft = resolvedRight = remaining * 0.5f;
				}
				else if (leftAuto) {
					float r = rightAuto ? 0.0f : childMargin.right;
					resolvedLeft = std::max(0.0f, remaining - r);
				}
				else if (rightAuto) {
					resolvedRight = std::max(0.0f, childMargin.left);
				}
				ch->layout.margin.left = resolvedLeft;
				ch->layout.margin.right = resolvedRight;
				ch->layout.marginLeftAuto = false;
				ch->layout.marginRightAuto = false;
				ch->layout.finalizeSizes();
			}
		}
		BlockLayout::layoutBlock(this, node, contentAbsoluteX, contentAbsoluteY);
	}

	calculateScrollState(node);
	// mark clean
	node->clearDirty(ldt::DirtyFlag::Layout | ldt::DirtyFlag::ChildrenLayout);
}
void BoxModelEngine::reMeasureChildren(ldt::ResolvedNode* node) {
    if (!node) return;
	float w = node->layout.getContentBox().width;
	float h = node->layout.getContentBox().height;

	// convenience refs
	ldt::PropertyResolver resolver(node);
	const IPropertyProvider* prop = &resolver;
	auto& cl = node->layout;


	ldt::FormattingContext ctx = ldt::FormattingContext::Block;
    if (prop->getDisplay() == ldt::FormattingContext::Flex) ctx = ldt::FormattingContext::Flex;
    else if (prop->getDisplay() == ldt::FormattingContext::Grid) ctx = ldt::FormattingContext::Grid;
    else if (prop->getDisplay() == ldt::FormattingContext::Inline) ctx = ldt::FormattingContext::Block;

    // Dispatch
    if (ctx == ldt::FormattingContext::Flex) {
        FlexLayout::measureFlex(this, node, w, h, w, h);
    } else if (ctx == ldt::FormattingContext::Inline) {
        InlineLayout::measureInline(this, node, w, h, w, h);
    } else {
        BlockLayout::measureBlock(this, node, w, h, w, h);
    }
}


std::type_index BoxModelEngine::type() const
{
	return typeid(BoxModelEngine);
}

void BoxModelEngine::process(std::shared_ptr<ASTNode> root, ldt::DisplayList& outList, 
                             float viewportW, float viewportH) {
	if (!root) return;

	if (!getContext()) return;
	ldt::ResolvedTree* rt = getContext()->getResolvedTree();
	if (!rt) return;

	// ResolvedTree construction is handled by DocumentRuntime; assume rt already built/updated.

	auto rootNode = rt->getRoot();
	if (!rootNode) return;

	bool layoutDirty = false;
	if (rootNode->isDirty(ldt::DirtyFlag::Layout) || rootNode->isDirty(ldt::DirtyFlag::ChildrenLayout)) {
		layoutDirty = true;
	}
	// Also check if viewport changed size, which forces layout
	// (Assuming we track last viewport size, but for now just log if we are doing layout)
	
	//since we always do full layout in this engine implementation:
    // Debug: Print tree and dirty flags
	//LDT_LOG("[BoxModelEngine] Layout calculation started (Viewport: " << viewportW << "x" << viewportH << ")");

    //std::cout << "=== Resolved Tree Dirty State ===" << std::endl;
    std::function<void(ldt::ResolvedNode*, int)> printTree = [&](ldt::ResolvedNode* node, int depth) {
        if (!node) return;
        std::string indent(depth * 2, ' ');
		std::string type = node->astNode ? node->astNode->type : "Unknown";
		std::string uid = node->astNode->getUid()->as<std::string>();
        std::cout << indent << type << "(" << uid << ")" << " : " << dirtyFlagToString(node->getDirtyFlags()) << std::endl;
        for (auto* child : node->getChildren()) {
            printTree(child, depth + 1);
        }
    };
	std::function<void(ldt::ResolvedNode*, std::vector<ldt::ResolvedNode*>&)> getOverlayTree = [&](ldt::ResolvedNode* node, std::vector<ldt::ResolvedNode*>& data) {
		if (!node) return;
		for (auto* child : node->getOverlayChildren()) {
			data.push_back(child);
		}
		for (auto* child : node->getChildren()) {
			getOverlayTree(child, data);
		}
		};
	//printTree(rootNode, 0);
    //std::cout << "=================================" << std::endl;


	rootNode->finalStyle.width = ldt::LayoutUnit::px((float)viewportW);
	rootNode->finalStyle.height = ldt::LayoutUnit::px((float)viewportH);

	// top-level container: content area equals viewport
	std::vector<ldt::ResolvedNode*> data;
	getOverlayTree(rootNode, data);

	// ── Measure pass ──
	{ PERF_SCOPE("measure");
	measurePhase(rootNode, (float)viewportW, (float)viewportH,
		ldt::FormattingContext::Block);
	for (auto& var : data)
	{
		measurePhase(var, (float)viewportW, (float)viewportH,
			ldt::FormattingContext::Block);
	}
	}

	// ── Layout pass ──
	{ PERF_SCOPE("layout");
	layoutPhase(rootNode, 0.0f, 0.0f);
	for (auto& var : data)
	{
		layoutPhase(var, 0, 0);
	}
	}

    //std::cout << "=== Resolved Tree Dirty State ===" << std::endl;
	//printTree(rootNode, 0);
    //std::cout << "=================================" << std::endl;
}

void BoxModelEngine::onASTUpdated(std::shared_ptr<ASTNode> root) {
	// BoxModelEngine 不直接更新 ResolvedTree；ResolvedTree 的增量更新由 ASTBuildEngine 负责。
	// 此处保留空实现以响应 AST 更新（如果未来需要可在此处清理 BoxModelEngine 缓存）。
	(void)root;
	(void)getContext();
}

void BoxModelEngine::calculateScrollState(ldt::ResolvedNode* node) {
    if (!node) return;

	auto& l = node->layout;
	ldt::PropertyResolver resolver(node);
    const IPropertyProvider* prop = &resolver;
    /* ========= Phase 1: Content Scroll Size ========= */

	const auto contentAbsBox = l.getAbsoluteContentBox();
	float contentAbsX = contentAbsBox.x;
	float contentAbsY = contentAbsBox.y;

	float maxR = 0;
	float maxB = 0;

	for (const auto& child : node->getFlowChildren()) {
		ldt::PropertyResolver childResolver(child);
		const IPropertyProvider* childProp = &childResolver;
		if (childProp->getDisplay() == ldt::FormattingContext::None) continue;

		maxR = std::max(maxR,
			(child->layout.getMarginBox().x + child->layout.getMarginBox().width) - contentAbsX);
		maxB = std::max(maxB,
			(child->layout.getMarginBox().y + child->layout.getMarginBox().height) - contentAbsY);
	}

    /* ========= Phase 2: Initial Viewport ========= */

	l.viewportWidth  = l.getPaddingBox().width;
	l.viewportHeight = l.getPaddingBox().height;

    if (prop->getOverflow() == ldt::Overflow::Visible) {
        // overflow=visible: 不创建 viewport，内容可溢出且不做视口裁剪
        l.viewportWidth  = 0;
        l.viewportHeight = 0;
        l.scroll.hasHBar = false;
        l.scroll.hasVBar = false;
    } else if (prop->getOverflow() == ldt::Overflow::Hidden) {
        l.scroll.hasHBar = l.scroll.hasVBar = false;
    } else {
		ui::Edges nodePadding = prop->getPadding();
		l.scroll.scrollWidth = nodePadding.left + maxR + nodePadding.right;
		l.scroll.scrollHeight = nodePadding.top + maxB + nodePadding.bottom;

        const float SB = ldt::ScrollbarControl::kScrollbarThickness;
        bool force = (prop->getOverflow() == ldt::Overflow::Scroll);


        // max 2 passes
        for (int i = 0; i < 2; ++i) {
            bool needH = force || (ldt::floatGreater(l.scroll.scrollWidth, l.viewportWidth));
            bool needV = force || (ldt::floatGreater(l.scroll.scrollHeight, l.viewportHeight));

			float newVW = l.getPaddingBox().width  - (needV ? SB : 0);
			float newVH = l.getPaddingBox().height - (needH ? SB : 0);

            if (newVW < 0) newVW = 0;
            if (newVH < 0) newVH = 0;
			if (needH != needV)
			{
				break;
			}
            bool stable =
                (needH == l.scroll.hasHBar) &&
                (needV == l.scroll.hasVBar);

            l.scroll.hasHBar = needH;
            l.scroll.hasVBar = needV;
            l.viewportWidth  = newVW;
            l.viewportHeight = newVH;

            if (stable) break;
        }
    }

    /* ========= Phase 3: Scroll Range ========= */

    auto maxScrollX = std::max(0.0f, l.scroll.scrollWidth  - l.viewportWidth);
	auto maxScrollY = std::max(0.0f, l.scroll.scrollHeight - l.viewportHeight);

    // Sync from Control if available (User Input Source of Truth)
    if (auto ctrl = node->getControl().lock()) {
		l.scroll.offsetX = ctrl->getScrollX();
		l.scroll.offsetY = ctrl->getScrollY();
		//TODO 现在只设置ContainerControl，避免类似input另外挂载滚动条的控件被清除
		if (auto container = std::dynamic_pointer_cast<ldt::ContainerControl>(ctrl)) {
			l.scroll.offsetX = clampf(l.scroll.offsetX, 0.0f, maxScrollX);
			l.scroll.offsetY = clampf(l.scroll.offsetY, 0.0f, maxScrollY);
		}
    }

}

