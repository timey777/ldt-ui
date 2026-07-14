#include "scrollbar_control.h"
#include "scroll_helper.h"
#include "engine/document_runtime.h"
#include "engine/update_scheduler.h"
#include "scene.h"
#include "abstract_control.h"
#include "engine/ui_event_system.h"
#include <algorithm>

// Local epsilon; don't depend on ContainerControl
static constexpr float kScrollEpsilon = 0.01f;

namespace ldt {

bool ScrollbarControl::onParentLocalMouseMove(ParentLocalPointDp point) {
    return handleParentLocalMouseMove(point);
}

bool ScrollbarControl::onParentLocalMouseButton(ParentLocalPointDp point, int button, int action) {
    return handleParentLocalMouseButton(point, button, action);
}

void ScrollbarControl::onRender(DisplayList& displayList, const ui::Rect& clip) const {
    auto* parent = dynamic_cast<AbstractControl*>(getParent());
    if (!parent) return;

    const float barSize = kScrollbarThickness;
    ui::Color thumbColor(0.4f, 0.4f, 0.4f, 0.6f);

    float viewW = parent->getViewportWidth();
    float viewH = parent->getViewportHeight();
    const auto& sw = parent->getStrokeWidth();

    if (orient_ == Orientation::Vertical) {
        // track 应当等于 viewport 高度（viewport 已排除 stroke），无需再次减去 stroke
        float trackH = viewH;
        auto info = ScrollHelper::compute(viewH, parent->getScrollHeight(), parent->getScrollY(), kMinThumbSize, trackH);
        float thumbH = info.thumbSize;
        float ratio = info.ratio;

        float thumbY = parent->getBounds().y + sw.top + ratio * (trackH - thumbH);

        // Draw at right edge of content area (inset by stroke width)
        ui::Rect thumbRect(parent->getBounds().x + parent->getBounds().width - sw.right - barSize,
                           thumbY, barSize, thumbH);
        displayList.addRect(thumbRect, thumbColor, ui::Color(), 0, barSize/2);
    } else {
        // track 应当等于 viewport 宽度（viewport 已排除 stroke），无需再次减去 stroke
        float trackW = viewW;
        auto info = ScrollHelper::compute(viewW, parent->getScrollWidth(), parent->getScrollX(), kMinThumbSize, trackW);
        float thumbW = info.thumbSize;
        float ratio = info.ratio;

        float thumbX = parent->getBounds().x + sw.left + ratio * (trackW - thumbW);

        // Draw at bottom edge of content area (inset by stroke width)
        ui::Rect thumbRect(thumbX,
                           parent->getBounds().y + parent->getBounds().height - sw.bottom - barSize,
                           thumbW, barSize);
        displayList.addRect(thumbRect, thumbColor, ui::Color(), 0, barSize/2);
    }
}

bool ScrollbarControl::handleParentLocalMouseButton(ParentLocalPointDp point, int button, int action) {
    auto* parent = dynamic_cast<AbstractControl*>(getParent());
    if (!parent) return false;

    // point 已经是相对于 Parent 的 Viewport 坐标（因为是通过 onOverlayMouseButton 分发的，
    // 且 HitTestHelper 没有介入 pathCoords 的滚动累加，或者 pathCoords 对 Container 本身只计算了 transform/scroll of ancestors）
    float localX = point.x.value;
    float localY = point.y.value;

    const float barSize = kScrollbarThickness;
    float viewW = parent->getViewportWidth();
    float viewH = parent->getViewportHeight();
    const auto& sw = parent->getStrokeWidth();

    // 使用相对于 Container 左上角的本地坐标系
    
    if (action == 1) { // press
        if (orient_ == Orientation::Vertical) {
            float scrollH = parent->getScrollHeight();
            // track 应当等于 viewport 高度（viewport 已排除 stroke），无需再次减去 stroke
            float trackH = viewH;
                if (scrollH > kScrollEpsilon && ldt::floatGreater(scrollH, viewH, kScrollEpsilon)) {
                    auto info = ScrollHelper::compute(viewH, parent->getScrollHeight(), parent->getScrollY(), kMinThumbSize, trackH);
                    if (info.canScroll) {
                        float thumbH = info.thumbSize;
                        float ratio = info.ratio;
                        float thumbY = sw.top + ratio * (trackH - thumbH);

                        ui::Rect thumbRect(parent->getBounds().width - sw.right - barSize, thumbY, barSize, thumbH);
                        if (localX >= thumbRect.x && localX <= thumbRect.x + thumbRect.width && localY >= thumbRect.y && localY <= thumbRect.y + thumbRect.height) {
                            dragging_ = true;
                            dragStartPos_ = localY;
                            dragStartScroll_ = parent->getScrollY();
                            captureMouse(parent);
                            return true;
                        }
                    }
                } else {
                    // content fits -> show-only, do not start dragging
                }
        } else {
            // 水平滚动条逻辑
            float scrollW = parent->getScrollWidth();
            // track 应当等于 viewport 宽度（viewport 已排除 stroke），无需再次减去 stroke
            float trackW = viewW;
            if (scrollW > kScrollEpsilon && ldt::floatGreater(scrollW, viewW, kScrollEpsilon)) {
                auto info = ScrollHelper::compute(viewW, parent->getScrollWidth(), parent->getScrollX(), kMinThumbSize, trackW);
                if (info.canScroll) {
                    float thumbW = info.thumbSize;
                    float ratio = info.ratio;

                    float thumbX = sw.left + ratio * (trackW - thumbW);

                    ui::Rect thumbRect(thumbX, parent->getBounds().height - sw.bottom - barSize, thumbW, barSize);
                    if (localX >= thumbRect.x && localX <= thumbRect.x + thumbRect.width && localY >= thumbRect.y && localY <= thumbRect.y + thumbRect.height) {
                        dragging_ = true;
                        dragStartPos_ = localX;
                        dragStartScroll_ = parent->getScrollX();
                        captureMouse(parent);
                        return true;
                    }
                }
            } else {
                // content fits -> show-only, do not start dragging
            }
        }
    }

    if (action == 0) { // release
        if (dragging_) {
            dragging_ = false;
            releaseMouse();
            return true;
        }
    }

    return false;
}

bool ScrollbarControl::handleParentLocalMouseMove(ParentLocalPointDp point) {
    if (!dragging_) return false;
    auto* parent = dynamic_cast<AbstractControl*>(getParent());
    if (!parent) return false;

    // point 已经是相对于 Parent Viewport 的坐标 (由 captureMouse(parent) 保证)
    float localX = point.x.value;
    float localY = point.y.value;

    float viewW = (parent->getViewportWidth() > 0) ? parent->getViewportWidth() : parent->getBounds().width;
    float viewH = (parent->getViewportHeight() > 0) ? parent->getViewportHeight() : parent->getBounds().height;
    const auto& sw = parent->getStrokeWidth();

    if (orient_ == Orientation::Vertical) {
        // track 应当等于 viewport 高度（viewport 已排除 stroke），无需再次减去 stroke
        float trackH = viewH;
        auto info = ScrollHelper::compute(viewH, parent->getScrollHeight(), parent->getScrollY(), kMinThumbSize, trackH);
        if (info.trackMovable > 0.0f && info.maxScroll > 0.0f) {
            float delta = localY - dragStartPos_;
            float frac = delta / info.trackMovable;
            float newScrollY = dragStartScroll_ + frac * info.maxScroll;
            newScrollY = std::max(0.0f, std::min(newScrollY, info.maxScroll));
            if (ldt::floatNotEqual(newScrollY, parent->getScrollY())) {
                if(ldt::ScrollHelper::clampAndApply(parent, viewW, viewH, parent->getScrollWidth(), parent->getScrollHeight(), parent->getScrollX(), newScrollY))
                {
                    if (parent->getScene()) UpdateScheduler::getInstance().requestPaint();
                    if (parent->getScene() && parent->getScene()->getUIEventSystem()) {
                        UIEventSystem::UIEvent ev;
                        ev.type = UIEventSystem::UIEventType::Scroll;
                        ev.target = parent;
                        parent->getScene()->getUIEventSystem()->dispatch(ev);
                    }
                }
            }
        }
    } else {
        // track 应当等于 viewport 宽度（viewport 已排除 stroke），无需再次减去 stroke
        float trackW = viewW;
        auto info = ScrollHelper::compute(viewW, parent->getScrollWidth(), parent->getScrollX(), kMinThumbSize, trackW);
        if (info.trackMovable > 0.0f && info.maxScroll > 0.0f) {
            float delta = localX - dragStartPos_;
            float frac = delta / info.trackMovable;
            float newScrollX = dragStartScroll_ + frac * info.maxScroll;
            newScrollX = std::max(0.0f, std::min(newScrollX, info.maxScroll));
            if (ldt::floatNotEqual(newScrollX, parent->getScrollX())) {
                if (ldt::ScrollHelper::clampAndApply(parent, viewW, viewH, parent->getScrollWidth(), parent->getScrollHeight(), newScrollX, parent->getScrollY()))
                {
                    if (parent->getScene()) UpdateScheduler::getInstance().requestPaint();
                    if (parent->getScene() && parent->getScene()->getUIEventSystem()) {
                        UIEventSystem::UIEvent ev;
                        ev.type = UIEventSystem::UIEventType::Scroll;
                        ev.target = parent;
                        parent->getScene()->getUIEventSystem()->dispatch(ev);
                    }
                }
            }
        }
    }
    return true;
}

} // namespace ldt
