#include "opengl_drawer_factory.h"

#include "opengl_drawer_android.h"
#include "opengl_drawer_impl.h"

std::unique_ptr<IGraphicsContext> createOpenGLGraphicsContext() {
#if defined(__ANDROID__)
	return std::make_unique<OpenGLDrawerAndroid>();
#else
	return std::make_unique<OpenGLDrawerImpl>();
#endif
}
