#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include "graphics_types.h"
#include "text_layout.h"
#include "ldt_export.h"

class LDT_API IDrawer {
public:
    virtual ~IDrawer() = default;

    virtual void drawRect(float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) = 0;
    virtual void drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style = DrawStyle()) = 0;
    virtual void drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) = 0;

    // drawText: text drawn at (x,y). maxWidth <=0 means no wrapping.
    // Alignment/wrap/lineHeight are supplied via TextLayoutParams.
    // 注意：对于频繁变动或需要交互（光标、选区）的文本，建议使用 createTextLayout + drawTextLayout
    virtual void drawText(const std::string& text, float x, float y, const Font& font, const DrawStyle& style = DrawStyle(), float maxWidth = -1.0f, const TextLayoutParams& layout = TextLayoutParams()) = 0;

    /**
     * @brief 创建文本布局对象
     * 
     * 这是一个工厂方法，用于预计算文本布局。
     * 
     * @param text 文本内容
     * @param font 字体样式
     * @param params 布局参数（对齐、换行等）
     * @return std::shared_ptr<ITextLayout> 布局对象
     */
    virtual std::shared_ptr<ITextLayout> createTextLayout(const std::string& text, const Font& font, const TextLayoutParams& params, float maxWidth = -1.0f) = 0;

    /**
     * @brief 绘制预计算的文本布局
     * 
     * 性能优化点：
     * 1. 避免了每帧重复计算字形位置。
     * 2. 实现层可以利用 ITextLayout 中缓存的字形数据进行实例化绘制 (Instanced Rendering)。
     *    例如，将所有字形的 (textureID, position, size, color) 打包成 Instance Buffer，
     *    一次 DrawCall 完成绘制。
     * 
     * @param layout 由 createTextLayout 创建的布局对象
     * @param x 绘制起始 x 坐标
     * @param y 绘制起始 y 坐标
     * @param style 绘制样式（颜色等）
     */
    virtual void drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style) = 0;

    virtual ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) = 0;
    virtual void unloadImage(ImageHandle img) = 0;

    virtual void setClipRect(const render::Rect& r) = 0;
    virtual bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) = 0;
    virtual bool getClipRect(render::Rect* outCurrent) const = 0;
    virtual void resetClip() = 0;

    // Set the corner radius for the current clip region.
    // When radius > 0, fragment shaders will use SDF to discard fragments
    // outside the rounded rectangle defined by the current clip rect + radius.
    // Set to 0 to disable SDF clipping (scissor-only).
    virtual void setClipCornerRadius(float radius) = 0;
    virtual float getClipCornerRadius() const = 0;

    virtual void pushTransform(float dx, float dy) = 0;
    virtual void popTransform() = 0;
};
