#pragma once

#include <string>
#include <cstdint>

using ImageHandle = uint32_t;
namespace render
{
	struct Color {
		float r = 0, g = 0, b = 0, a = 1;
	};

	struct Rect {
		float x = 0, y = 0, w = 0, h = 0;
	};

}
struct Font {
	std::string family;
	float size = 0.0f;
	bool bold = false;
	bool italic = false;
	// 字体属性仅包含字体相关信息；对齐/换行/行高等文本布局
	// 现在由独立的 TextLayoutParams 提供
};

// 文本布局参数：对齐/是否换行/行高等
struct TextLayoutParams {
	std::string textAlign = "left"; // "left"/"center"/"right"
	bool wrap = true; // 是否允许换行
	float lineHeight = 0.0f; // 行高（像素），<=0 表示使用默认
};

struct DrawStyle {
	render::Color fillColor;
	render::Color strokeColor;
	float opacity = 1.0f;
	float cornerRadius = 0.0f;

	struct {
		float top = 0.0f, right = 0.0f, bottom = 0.0f, left = 0.0f;
		
		bool any() const { return top > 0 || right > 0 || bottom > 0 || left > 0; }
		bool isUniform() const { return top == right && right == bottom && bottom == left; }
		float first() const { return top; }
	} strokeWidths;
};
