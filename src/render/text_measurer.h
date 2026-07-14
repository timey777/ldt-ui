#pragma once

#include <string>
#include <memory>

// 简单的文本度量结构
struct TextMetrics {
    float width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineHeight = 0.0f;
};

struct FontDesc {
    std::string family;
    float size = 12.0f;
};

// 文本测量接口：用于将测量职责从 IDrawer 中抽离
class ITextMeasurer {
public:
    virtual ~ITextMeasurer() = default;
    // measureText: measure text metrics. If wrapWidth > 0, measurement should
    // consider wrapping to that width and return total measured height in
    // TextMetrics::lineHeight. Default wrapWidth <= 0 means no wrapping.
    virtual TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) = 0;
    virtual TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) = 0;
    virtual float getLineHeight(const FontDesc& font) = 0;
};

// No default implementation here; concrete implementations may be
// provided by the rendering backend (IDrawer) or higher-level code.
