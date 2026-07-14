#pragma once

#include <string>
#include "ldt_export.h"

namespace ldt
{
    namespace ui
    {
        // 颜色结构
        struct LDT_API Color
        {
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

            Color() = default;
            Color(float r_, float g_, float b_, float a_ = 1.0f)
                : r(r_), g(g_), b(b_), a(a_) {}

            // 从十六进制字符串解析 (#RRGGBB 或 #RRGGBBAA)
            static Color fromHex(const std::string &hex);
        };

        // 矩形区域
        struct LDT_API Rect
        {
            float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;

            Rect() = default;
            Rect(float x_, float y_, float w_, float h_)
                : x(x_), y(y_), width(w_), height(h_) {}

            // Edge accessors
            float right()  const { return x + width; }
            float bottom() const { return y + height; }

            // Check if rect has zero or negative area
            bool isEmpty() const { return width <= 0.0f || height <= 0.0f; }

            // Check if this rect overlaps with another rect
            bool intersects(const Rect& other) const {
                if (isEmpty() || other.isEmpty()) return false;
                return !(right() <= other.x || x >= other.right() ||
                         bottom() <= other.y || y >= other.bottom());
            }

            // Compute the intersection of two rects (returns empty rect if no overlap)
            Rect intersection(const Rect& other) const {
                if (!intersects(other)) return Rect();
                float nx = (x > other.x) ? x : other.x;
                float ny = (y > other.y) ? y : other.y;
                float nr = (right()  < other.right())  ? right()  : other.right();
                float nb = (bottom() < other.bottom()) ? bottom() : other.bottom();
                if (nx >= nr || ny >= nb) return Rect();
                return Rect(nx, ny, nr - nx, nb - ny);
            }
        };

        struct LDT_API Edges
        {
            float top = 0.0f, right = 0.0f, bottom = 0.0f, left = 0.0f;

            Edges() = default;
            Edges(float all) : top(all), right(all), bottom(all), left(all) {}
            Edges(float vertical, float horizontal) : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
            Edges(float t, float r, float b, float l) : top(t), right(r), bottom(b), left(l) {}

            float &operator[](size_t i)
            {
                if (i == 0) return top;
                if (i == 1) return right;
                if (i == 2) return bottom;
                return left;
            }
            const float &operator[](size_t i) const
            {
                if (i == 0) return top;
                if (i == 1) return right;
                if (i == 2) return bottom;
                return left;
            }

            // 辅助工具：快速计算水平和垂直的总和
            float horizontal() const { return left + right; }
            float vertical() const { return top + bottom; }

            bool any() const { return top > 0.0f || right > 0.0f || bottom > 0.0f || left > 0.0f; }
        };
    }
}
