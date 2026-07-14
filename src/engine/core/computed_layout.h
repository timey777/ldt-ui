#pragma once

#include "render/display_list.h"
#include "node_flags.h"
#include <string>
#include <cfloat>

namespace ldt {

	// 布局算法配置（从 LayoutEngine 的 @layout 块获取）
	struct CompiledLayoutRules
	{
		// 布局算法类型
		FormattingContext displayEnum = FormattingContext::Block;
		
		void setDisplay(FormattingContext display)
		{
			this->displayEnum = display;
		}

		FormattingContext getDisplay() const
		{
			return this->displayEnum;
		}

		Position position = Position::Static;

		// Flex 布局属性
		FlexDirection flexDirection = FlexDirection::Row;
		AlignItems alignItems = AlignItems::Stretch;
		JustifyContent justifyContent = JustifyContent::FlexStart;
		FlexWrap flexWrap = FlexWrap::NoWrap;
		float flexGrow = 0;
		float flexShrink = 1;

		// Grid 布局属性
		std::string gridTemplateColumns = "";
		std::string gridTemplateRows = "";
		float gap = 0;

	private:
		// Deprecated string version, mapping to displayEnum
		std::string display = "block"; 
	};

	// 计算后的布局数据（由 BoxModelEngine 计算）
	struct ComputedLayout
	{
		// 盒模型尺寸 (Margins & Paddings & Borders)
		ui::Edges margin;
		ui::Edges padding;
		ui::Edges border;

		// flags indicating the corresponding margin was specified as 'auto'
		bool marginTopAuto = false, marginRightAuto = false, marginBottomAuto = false, marginLeftAuto = false;

		float minWidth = 0, minHeight = 0;
		float maxWidth = FLT_MAX, maxHeight = FLT_MAX;

		// 最终计算的盒子（像素值）contentBox(相对坐标), paddingBox(相对坐标), borderBox(绝对坐标), marginBox(绝对坐标)
	private:
		ldt::ui::Rect contentBox_; // 内容区域 (Relative: 相对于 borderBox 左上角的偏移)
		ldt::ui::Rect paddingBox_; // 包含内边距 (Relative: 相对于 borderBox 左上角的偏移)
		ldt::ui::Rect borderBox_;  // 包含边框 (Absolute: 相对于根/窗口的绝对坐标)
		ldt::ui::Rect marginBox_;  // 包含外边距 (Absolute: 相对于根/窗口的绝对坐标)
	public:
		// Accessors (Read-only for consumers)
		const ldt::ui::Rect &getContentBox() const;
		const ldt::ui::Rect &getPaddingBox() const;
		const ldt::ui::Rect &getBorderBox() const;
		const ldt::ui::Rect &getMarginBox() const;
		ldt::ui::Rect getAbsolutePaddingBox() const;
		ldt::ui::Rect getAbsoluteContentBox() const;

		// Internal calculation (Called by BoxModelEngine)
		void finalizeSizes();
		void setPosition(float absX, float absY);

		// 实际使用的尺寸（在 auto 情况下由 BoxModelEngine 计算）
		float computedWidth = 0;
		float computedHeight = 0;
		// 文字绘制
		bool wrap = false;

		/* ================== Scroll Viewport（滚动视口） ================== */
		//
		// viewport 是滚动容器的固有属性：它表示"可滚动内容透过哪个窗口可见"。
		// 只有 overflow != visible 的容器才拥有 viewport。
		// viewport 属于控件/节点本身，跨帧持久，由布局阶段计算。
		//
		// viewport 不是 clip：
		//   - viewport = 谁拥有？→ 滚动容器。表示滚动窗口。
		//   - clip     = 谁拥有？→ 没有谁。是 render() 的临时参数，递归传递。
		//
		// @see docs/VIEWPORT_VS_CLIP_DESIGN.md

		// 可视区域大小（初始 = paddingBox.size，滚动条出现后会扣除滚动条尺寸）
		float viewportWidth = 0;
		float viewportHeight = 0;

		/* ================== Scroll 状态 ================== */
		struct ScrollData
		{
			float offsetX = 0, offsetY = 0;
			float scrollWidth = 0, scrollHeight = 0;
			bool hasHBar = false, hasVBar = false;
		};

		// overflow != "visible" 且内容超出 viewport 时使用
		ScrollData scroll;
	};

} // namespace ldt
