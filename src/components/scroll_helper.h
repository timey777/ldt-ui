#pragma once

#include <algorithm>
#include <utility>
#include "abstract_control.h"
#include "misc/float_utils.h"

namespace ldt {

struct ScrollInfo {
    float thumbSize = 0.0f;    // 滑块尺寸（像素）
    float ratio = 0.0f;        // 滚动位置比例 [0,1]
    float maxScroll = 0.0f;    // 最大可滚动距离
    float trackMovable = 0.0f; // 轨道上滑块可移动距离
    bool canScroll = false;    // 是否可以滚动
};

class ScrollHelper {
public:
    // 统一计算滚动条 thumb 大小与位置信息
    // viewportSize: 可视区尺寸（宽或高）
    // contentSize: 内容总尺寸（宽或高）
    // scrollPos: 当前滚动位置（x 或 y）
    // minThumbSize: 最小滑块尺寸
    // trackSize: 轨道长度（通常等于 viewportSize）
    static ScrollInfo compute(float viewportSize, float contentSize, float scrollPos, float minThumbSize, float trackSize) {
        ScrollInfo info;
        info.maxScroll = std::max(0.0f, contentSize - viewportSize);
        float proportional = 0.0f;
        if (contentSize > 0.0f) proportional = trackSize * (viewportSize / contentSize);
        float maxThumb = trackSize;
        float minThumb = std::min(minThumbSize, maxThumb);
        info.thumbSize = std::clamp(proportional, minThumb, maxThumb);
        info.trackMovable = std::max(0.0f, trackSize - info.thumbSize);
        info.ratio = (info.maxScroll > 0.0f) ? (scrollPos / info.maxScroll) : 0.0f;
        info.canScroll = (info.maxScroll > 0.0f) && (info.trackMovable > 0.0f);
        return info;
    }

    // Clamp scroll positions for 2D content given viewport and content sizes.
    // viewportWidth/Height: 可视区尺寸
    // contentWidth/Height: 内容总尺寸
    // scrollX/scrollY: 当前滚动位置
    // 返回 pair<newScrollX, newScrollY>
    static std::pair<float,float> clampScroll(float viewportWidth, float viewportHeight,
                                              float contentWidth, float contentHeight,
                                              float scrollX, float scrollY) {
        float maxScrollX = std::max(0.0f, contentWidth - viewportWidth);
        float maxScrollY = std::max(0.0f, contentHeight - viewportHeight);
        float newScrollX = std::max(0.0f, std::min(scrollX, maxScrollX));
        float newScrollY = std::max(0.0f, std::min(scrollY, maxScrollY));
        return std::make_pair(newScrollX, newScrollY);
    }

    // Clamp and apply scroll to given control. Returns true if scroll was changed/applied.
    static bool clampAndApply(AbstractControl* ctrl, float viewportWidth, float viewportHeight,
                              float contentWidth, float contentHeight, float newScrollX, float newScrollY) {
        if (!ctrl) return false;
        auto p = clampScroll(viewportWidth, viewportHeight, contentWidth, contentHeight, newScrollX, newScrollY);
        if (ldt::floatNotEqual(p.first, ctrl->getScrollX()) || ldt::floatNotEqual(p.second, ctrl->getScrollY())) {
            ctrl->setScroll(p.first, p.second);
            ctrl->markLayerDirty();
            return true;
        }
        return false;
    }

};

} // namespace ldt
