#pragma once

#include <memory>

class IGraphicsContext;

std::unique_ptr<IGraphicsContext> createOpenGLGraphicsContext();
