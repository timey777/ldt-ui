#pragma once

#include "engine/core/coordinate_types.h"

class IDrawer;

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    virtual void init(int width, int height) = 0;
    virtual void resize(const ldt::FramebufferSizePx& framebufferSize,
                        const ldt::ContentScale& contentScale = {}) = 0;
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void present() = 0;
    virtual void shutdown() = 0;

    virtual IDrawer* getDrawer() = 0;
};
