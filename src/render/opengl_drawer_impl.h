#pragma once

#include "drawer.h"
#include "graphics_context.h"
#include "text_measurer.h"

// Do not use LDT_ENABLE_RENDERING in this header — the macro is a project/target
// level compile flag and must not affect public headers. Implementations that
// depend on rendering are selected in the .cpp file via that macro.

class LDT_API OpenGLDrawerImpl : public IDrawer, public IGraphicsContext, public ITextMeasurer {
public:
	OpenGLDrawerImpl();
	~OpenGLDrawerImpl() override;

	// IGraphicsContext implementation
	void init(int width, int height) override;
	void resize(const ldt::FramebufferSizePx& framebufferSize,
		const ldt::ContentScale& contentScale = {}) override;
	void clear(float r, float g, float b, float a) override;
	void present() override;
	void shutdown() override;
	IDrawer* getDrawer() override { return this; }

	// IDrawer implementation
	void drawRect(float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;
	void drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style = DrawStyle()) override;
	void drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;

	void drawText(const std::string& text, float x, float y, const Font& font, const DrawStyle& style = DrawStyle(), float maxWidth = -1.0f, const TextLayoutParams& layout = TextLayoutParams()) override;

	std::shared_ptr<ITextLayout> createTextLayout(const std::string& text, const Font& font, const TextLayoutParams& params, float maxWidth = -1.0f) override;
	void drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style) override;

	// ITextMeasurer overrides (allow DocumentRuntime to accept drawer as text measurer)
	TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) override;
	TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) override;
	float getLineHeight(const FontDesc& font) override;

	ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) override;
	void unloadImage(ImageHandle img) override;

	void setClipRect(const render::Rect& r) override;
	bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) override;
	bool getClipRect(render::Rect* outCurrent) const override;
	void resetClip() override;

	void setClipCornerRadius(float radius) override;
	float getClipCornerRadius() const override;

	void pushTransform(float dx, float dy) override;
	void popTransform() override;

public:
	class ImplData;

private:
	ImplData* d;
};
