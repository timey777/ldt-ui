#include "container_control.h"
#include "misc/logger.h"
#include "misc/float_utils.h"
#include "misc/render_stats.h"
#include "scene.h"
#include "engine/document_runtime.h"
#include "engine/update_scheduler.h"
#include "control_factory.h"
#include "scroll_helper.h"
#include <algorithm>

namespace ldt {

// ===================================================================
// Bounding Rect — subtree union of all child bounds
// ===================================================================
ui::Rect ContainerControl::getBoundingRect() const {
    if (!boundingRectDirty_) return cachedBoundingRect_;

    cachedBoundingRect_ = ui::Rect();
    bool first = true;
    for (const auto& child : children_) {
        if (!child || !child->isVisible()) continue;
        const ui::Rect& cb = child->getBounds();
        if (first) {
            cachedBoundingRect_ = cb;
            first = false;
        } else {
            float x1 = (cachedBoundingRect_.x < cb.x) ? cachedBoundingRect_.x : cb.x;
            float y1 = (cachedBoundingRect_.y < cb.y) ? cachedBoundingRect_.y : cb.y;
            float x2 = (cachedBoundingRect_.right() > cb.right()) ? cachedBoundingRect_.right() : cb.right();
            float y2 = (cachedBoundingRect_.bottom() > cb.bottom()) ? cachedBoundingRect_.bottom() : cb.bottom();
            cachedBoundingRect_ = ui::Rect(x1, y1, x2 - x1, y2 - y1);
        }
    }
    boundingRectDirty_ = false;
    return cachedBoundingRect_;
}

void ContainerControl::invalidateBoundingRect() {
    boundingRectDirty_ = true;
    // Propagate upward: parent's bounding rect also depends on this container
    if (auto* p = dynamic_cast<ContainerControl*>(parent_)) {
        p->invalidateBoundingRect();
    }
}

bool ContainerControl::handleLocalMouseMove(ControlLocalPointDp point) {
    (void)point;
    // EventDispatcher handles overlay (scrollbars) and path dispatch.
    // No need to manually iterate children or scrollbars here.
    return false;
}

bool ContainerControl::handleLocalMouseButton(ControlLocalPointDp point, int button, int action) {
    (void)point;
    (void)button;
    (void)action;
    // EventDispatcher handles overlay (scrollbars) via onOverlayMouseButton
    // and path dispatch for children.
    // No need to manually iterate children here.
    return false;
}

bool ContainerControl::onOverlayMouseButton(ParentLocalPointDp point, int button, int action) {
    // Viewport not yet set — nothing to do
    if (!hasViewport()) return false;
    float viewW = viewportWidth_;
    float viewH = viewportHeight_;

    if (!vScrollbar_ && (ldt::floatGreater(scrollHeight_, viewH, kScrollEpsilon))) {
           vScrollbar_ = ControlFactory::getInstance()->CreateScrollbar(ScrollbarControl::Orientation::Vertical);
        vScrollbar_->setParent(this);
        if (scene_) vScrollbar_->setScene(scene_);
    }
    if (!hScrollbar_ && (ldt::floatGreater(scrollWidth_, viewW, kScrollEpsilon))) {
           hScrollbar_ = ControlFactory::getInstance()->CreateScrollbar(ScrollbarControl::Orientation::Horizontal);
        hScrollbar_->setParent(this);
        if (scene_) hScrollbar_->setScene(scene_);
    }

    // Forward to overlay scrollbars first
    if (vScrollbar_ && vScrollbar_->onParentLocalMouseButton(point, button, action)) return true;
    if (hScrollbar_ && hScrollbar_->onParentLocalMouseButton(point, button, action)) return true;

    return false;
}

void ContainerControl::renderChildren(DisplayList& displayList, const ui::Rect& parentClip) const
{
    if (!hasViewport()) {
        renderNonScrollChildrenDirect(displayList, parentClip);
        renderScrollbarsIfNeeded(displayList);
        return;
    }

    // ── Inherit dragging state from parent, then check self ──
    bool savedDragging = renderDragging_;
    bool inherited = false;
    if (auto* p = dynamic_cast<ContainerControl*>(parent_))
        inherited = p->renderDragging_;
    renderDragging_ = inherited || (getVisualOffsetX() != 0.0f || getVisualOffsetY() != 0.0f);

    ui::Rect clip = effectiveClipRect(parentClip);

    bool scrollable = hasScrollableContent();

    if (renderDragging_) {
        if (scrollable)
            renderScrollChildrenDirect(displayList, clip);
        else
            renderNonScrollChildrenDirect(displayList, clip);
        renderScrollbarsIfNeeded(displayList);
    } else if (scrollable) {
        renderScrollChildrenCulled(displayList, clip);
    } else {
        renderNonScrollChildrenCulled(displayList, clip);
    }

    renderDragging_ = savedDragging;
}

// ═══════════════════════════════════════════════════════════════
//  renderChildren  helpers
// ═══════════════════════════════════════════════════════════════

ui::Rect ContainerControl::effectiveClipRect(const ui::Rect& parentClip) const {
    if (!parentClip.isEmpty())   return parentClip;
    // viewportWidth_/Height_ 是 paddingBox 尺寸（局部坐标），
    // 但子控件 bounds 是绝对坐标，所以需要返回 contentClipRect()（绝对坐标）。
    return contentClipRect();
}

bool ContainerControl::hasScrollableContent() const {
    return ldt::floatGreater(scrollWidth_,  viewportWidth_,  kScrollEpsilon) ||
           ldt::floatGreater(scrollHeight_, viewportHeight_, kScrollEpsilon);
}

ui::Rect ContainerControl::contentClipRect() const {
    ui::Rect c = bounds_;
    c.x     += strokeWidth_.left;
    c.y     += strokeWidth_.top;
    c.width  = std::max(0.0f, c.width  - strokeWidth_.left - strokeWidth_.right);
    c.height = std::max(0.0f, c.height - strokeWidth_.top  - strokeWidth_.bottom);
    return c;
}

void ContainerControl::pushContentClip(DisplayList& dl, const ui::Rect& clip) const {
    if (cornerRadius_ > 0.0f) {
        float s = std::max({strokeWidth_.left, strokeWidth_.right,
                            strokeWidth_.top, strokeWidth_.bottom});
        dl.pushClip(clip, std::max(0.0f, cornerRadius_ - s));
    } else {
        dl.pushClip(clip);
    }
}

bool ContainerControl::isChildVisible(const ui::Rect& clip, AbstractControl* child) const {
    if (!child) return false;
    if (renderDragging_) return true;
    if (child->getVisualOffsetX() != 0.0f || child->getVisualOffsetY() != 0.0f) return true;
    return child->getBounds().intersects(clip);
}

void ContainerControl::renderChildWithClip(DisplayList& dl, const ui::Rect& clip, AbstractControl* child) const {
    child->render(dl, clip.intersection(child->getBounds()));
}

void ContainerControl::renderScrollbarsIfNeeded(DisplayList& dl) const {
    float vw = viewportWidth_, vh = viewportHeight_;
    if (ldt::floatGreater(scrollHeight_, vh, kScrollEpsilon)) {
        if (!vScrollbar_) { vScrollbar_ = ControlFactory::getInstance()->CreateScrollbar(ScrollbarControl::Orientation::Vertical);   vScrollbar_->setParent(const_cast<ContainerControl*>(this)); if (scene_) vScrollbar_->setScene(scene_); }
        if (vScrollbar_) vScrollbar_->render(dl, ui::Rect());
    }
    if (ldt::floatGreater(scrollWidth_, vw, kScrollEpsilon)) {
        if (!hScrollbar_) { hScrollbar_ = ControlFactory::getInstance()->CreateScrollbar(ScrollbarControl::Orientation::Horizontal); hScrollbar_->setParent(const_cast<ContainerControl*>(this)); if (scene_) hScrollbar_->setScene(scene_); }
        if (hScrollbar_) hScrollbar_->render(dl, ui::Rect());
    }
}

void ContainerControl::renderScrollChildrenDirect(DisplayList& dl, const ui::Rect& clip) const {
    ui::Rect cc = contentClipRect();
    pushContentClip(dl, cc);
    dl.pushTransform(-getScrollX(), -getScrollY());
    for (const auto& child : children_) {
        if (!child) continue;
        if (g_currentRenderStats) {
            g_currentRenderStats->visitedNodes++;
            g_currentRenderStats->visibleNodes++;
        }
        child->render(dl, clip);
    }
    dl.popTransform();
    dl.popClip();
    if (g_currentRenderStats) {
        g_currentRenderStats->culledNodes = g_currentRenderStats->visitedNodes - g_currentRenderStats->visibleNodes;
    }
}

void ContainerControl::renderNonScrollChildrenDirect(DisplayList& dl, const ui::Rect& clip) const {
    // overflow=visible 时即使有圆角也不裁剪
    if (hasViewport() && cornerRadius_ > 0.0f) {
        pushContentClip(dl, contentClipRect());
        for (const auto& child : children_) {
            if (!child) continue;
            child->render(dl, clip);
        }
        dl.popClip();
    } else {
        for (const auto& child : children_) {
            if (!child) continue;
            child->render(dl, clip);
        }
    }
}

void ContainerControl::renderScrollChildrenCulled(DisplayList& dl, const ui::Rect& clip) const {
    float vw = viewportWidth_, vh = viewportHeight_;

    // Invalidate cache when scroll moves too far
    if (layerDisplayList_ && !isLayerDirty_) {
        float dy = getScrollY() - cachedScrollY_; if (dy < 0) dy = -dy;
        float dx = getScrollX() - cachedScrollX_; if (dx < 0) dx = -dx;
        if (dy > vh || dx > vw) isLayerDirty_ = true;
    }

    // Rebuild cached layer
    if (isLayerDirty_ || !layerDisplayList_) {
        if (!layerDisplayList_) layerDisplayList_ = std::make_shared<DisplayList>();
        layerDisplayList_->clear();

        ui::Rect padded(clip.x - vw, clip.y - vh, clip.width + vw * 2, clip.height + vh * 2);

        size_t first = 0, last = children_.size();
        if (children_.size() > 50) {
            first = std::lower_bound(children_.begin(), children_.end(), padded.y,
                [](const auto& c, float y) { return c && c->getBounds().bottom() <= y; }) - children_.begin();
            last  = std::upper_bound(children_.begin(), children_.end(), padded.bottom(),
                [](float y, const auto& c) { return y <= (c ? c->getBounds().y : 0.f); }) - children_.begin();
            if (first > 0) first--;
            if (last < children_.size()) last++;
        }

        for (size_t i = first; i < last; ++i) {
            const auto& child = children_[i];
            if (!child) continue;
            if (g_currentRenderStats) g_currentRenderStats->visitedNodes++;
            if (!child->getBounds().intersects(padded)) continue;
            if (g_currentRenderStats) g_currentRenderStats->visibleNodes++;
            renderChildWithClip(*layerDisplayList_, padded, child.get());
        }
        if (g_currentRenderStats) {
            g_currentRenderStats->culledNodes = g_currentRenderStats->visitedNodes - g_currentRenderStats->visibleNodes;
        }
        isLayerDirty_ = false;
        cachedScrollX_ = getScrollX();
        cachedScrollY_ = getScrollY();
        cachedContentClip_ = contentClipRect();
    }

    ui::Rect cc = cachedContentClip_.isEmpty() ? contentClipRect() : cachedContentClip_;
    pushContentClip(dl, cc);
    dl.pushTransform(-getScrollX(), -getScrollY());
    dl.addLayer(layerDisplayList_);
    dl.popTransform();
    dl.popClip();
    renderScrollbarsIfNeeded(dl);
}

void ContainerControl::renderNonScrollChildrenCulled(DisplayList& dl, const ui::Rect& clip) const {
    pushContentClip(dl, contentClipRect());
    for (const auto& child : children_) {
        if (!child) continue;
        if (g_currentRenderStats) g_currentRenderStats->visitedNodes++;
        if (!isChildVisible(clip, child.get())) continue;
        if (g_currentRenderStats) g_currentRenderStats->visibleNodes++;
        renderChildWithClip(dl, clip, child.get());
    }
    dl.popClip();
    if (g_currentRenderStats) {
        g_currentRenderStats->culledNodes = g_currentRenderStats->visitedNodes - g_currentRenderStats->visibleNodes;
    }
}

bool ContainerControl::handleLocalScroll(ControlLocalScrollDeltaDp delta) {
    // Viewport not yet set — nothing to scroll
    if (!hasViewport()) return false;
    float viewW = viewportWidth_;
    float viewH = viewportHeight_;

    if (ldt::floatGreater(scrollWidth_, viewW) || ldt::floatGreater(scrollHeight_, viewH)) {
        float newScrollX = getScrollX() - delta.dx.value * 20.0f;
        float newScrollY = getScrollY() - delta.dy.value * 20.0f;

        if (ldt::ScrollHelper::clampAndApply(this, viewW, viewH, scrollWidth_, scrollHeight_, newScrollX, newScrollY))
        {
            if (scene_) {
                // 仅请求重绘，不重新计算样式/布局
                UpdateScheduler::getInstance().requestPaint();
            }
            return true;
        }
    }
    return false;
}

} // namespace ldt
