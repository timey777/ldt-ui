#pragma once

namespace ldt {

struct CssPx {
    float value = 0.0f;
};

struct LogicalPx {
    float value = 0.0f;
};

struct DeviceScaleFactor {
    float value = 1.0f;
};

struct PageScaleFactor {
    float value = 1.0f;
};

struct FramebufferSizePx {
    int width = 0;
    int height = 0;
};

struct ViewportSizeDp {
    LogicalPx width;
    LogicalPx height;
};

struct DevicePointPx {
    float x = 0.0f;
    float y = 0.0f;
};

struct LogicalPointDp {
    LogicalPx x;
    LogicalPx y;
};

struct NodeLocalPointDp {
    LogicalPx x;
    LogicalPx y;
};

struct ControlLocalPointDp {
    LogicalPx x;
    LogicalPx y;
};

struct ParentLocalPointDp {
    LogicalPx x;
    LogicalPx y;
};

inline NodeLocalPointDp makeNodeLocalPoint(float x, float y) {
    return { { x }, { y } };
}

inline ParentLocalPointDp makeParentLocalPoint(float x, float y) {
    return { { x }, { y } };
}

inline ControlLocalPointDp makeControlLocalPoint(float x, float y) {
    return { { x }, { y } };
}

inline ParentLocalPointDp toParentLocalPoint(NodeLocalPointDp point) {
    return { point.x, point.y };
}

inline ParentLocalPointDp toParentLocalPoint(ControlLocalPointDp point) {
    return { point.x, point.y };
}

inline ControlLocalPointDp toControlLocalPoint(NodeLocalPointDp point) {
    return { point.x, point.y };
}

struct DeviceScrollDeltaPx {
    float dx = 0.0f;
    float dy = 0.0f;
};

struct LogicalScrollDeltaDp {
    LogicalPx dx;
    LogicalPx dy;
};

struct LogicalDragOffsetDp {
    LogicalPx dx;
    LogicalPx dy;
};

inline LogicalDragOffsetDp makeLogicalDragOffset(float dx, float dy) {
    return { { dx }, { dy } };
}

struct ControlLocalScrollDeltaDp {
    LogicalPx dx;
    LogicalPx dy;
};

struct ContentScale {
    DeviceScaleFactor x;
    DeviceScaleFactor y;
};

inline DeviceScaleFactor sanitizeScale(DeviceScaleFactor scale) {
    return { scale.value > 0.0f ? scale.value : 1.0f };
}

inline ContentScale sanitizeScale(ContentScale scale) {
    return { sanitizeScale(scale.x), sanitizeScale(scale.y) };
}

inline LogicalPx toLogicalPx(float physicalPx, DeviceScaleFactor scale) {
    const DeviceScaleFactor safeScale = sanitizeScale(scale);
    return { physicalPx / safeScale.value };
}

inline ViewportSizeDp toViewportSizeDp(FramebufferSizePx framebufferSize, ContentScale scale) {
    const ContentScale safeScale = sanitizeScale(scale);
    return {
        toLogicalPx(static_cast<float>(framebufferSize.width), safeScale.x),
        toLogicalPx(static_cast<float>(framebufferSize.height), safeScale.y)
    };
}

inline LogicalPointDp toLogicalPointDp(DevicePointPx point, ContentScale scale) {
    const ContentScale safeScale = sanitizeScale(scale);
    return {
        toLogicalPx(point.x, safeScale.x),
        toLogicalPx(point.y, safeScale.y)
    };
}

inline LogicalScrollDeltaDp toLogicalScrollDeltaDp(DeviceScrollDeltaPx delta, ContentScale scale) {
    const ContentScale safeScale = sanitizeScale(scale);
    return {
        toLogicalPx(delta.dx, safeScale.x),
        toLogicalPx(delta.dy, safeScale.y)
    };
}

} // namespace ldt