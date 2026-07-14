#pragma once

#include "abstract_control.h"
#include "engine/resource_manager.h"
#include "render/text_layout.h"
#include "render/text_measurer.h"
#include "render/drawer.h"
#include "scene.h"
#include "engine/core/node_flags.h"
#include <cmath>

namespace ldt {

/**
 * @brief 文本控件
 * 
 * 用于显示文本内容
 */
class LDT_API Text : public AbstractControl {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Text() = default;
    explicit Text(const std::string& text) : text_(text) {}
    friend class ControlFactory;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override {
        if (text_.empty()) return;

        // 只有当有背景色或有边框时才绘制矩形背景
        bool hasFill = (fillColor_.a > 0.0f);
        bool hasStroke = (strokeWidth_.any() && strokeColor_.a > 0.0f);

        if (hasFill || hasStroke) {
            displayList.addRect(
                bounds_,
                fillColor_,
                strokeColor_,
                strokeWidth_,
                cornerRadius_
            );
        }

        if (!backgroundImage_.empty()) {
            displayList.addImage(backgroundImage_, bounds_);
        }

        // 使用专用的文本颜色（textColor_），如果没有设置则使用默认黑色
        ui::Color textColor = textColor_;
        if (textColor.a == 0.0f) {
            textColor = ui::Color(0.0f, 0.0f, 0.0f, 1.0f);
        }

        float innerW = bounds_.width - (paddingLeft_ + paddingRight_);
        if (wrap_ && std::abs(innerW - lastLayoutWidth_) > 0.1f) {
             layoutDirty_ = true;
        }

        if (layoutDirty_ || !textLayout_) {
            const_cast<Text*>(this)->updateTextLayout();
        }

        if (textLayout_) {
            float startX = bounds_.x + paddingLeft_;
            float startY = bounds_.y + paddingTop_;
            float contentHeight = textLayout_->getBounds().h;
            
            displayList.addTextLayout(textLayout_, ui::Rect(startX, startY, innerW, contentHeight), textColor);
        }
    }

public:
    std::string getTypeName() const override { return "Text"; }

    // Padding
    void setPadding(float left, float top, float right, float bottom) {
        paddingLeft_ = left; paddingTop_ = top; paddingRight_ = right; paddingBottom_ = bottom;
        layoutDirty_ = true;
    }

    // 文本相关属性
    void setText(const std::string& text);
    const std::string& getText() const { return text_; }

    void setFontSize(float size) { 
        if (fontSize_ != size) {
            fontSize_ = size; 
            layoutDirty_ = true;
        }
    }
    float getFontSize() const { return fontSize_; }

    void setFontFamily(const std::string& family) { 
        if (fontFamily_ != family) {
            fontFamily_ = family; 
            layoutDirty_ = true;
        }
    }
    const std::string& getFontFamily() const { return fontFamily_; }

    void setBold(bool bold) { 
        if (bold_ != bold) {
            bold_ = bold; 
            layoutDirty_ = true;
        }
    }
    bool isBold() const { return bold_; }

    void setItalic(bool italic) { 
        if (italic_ != italic) {
            italic_ = italic; 
            layoutDirty_ = true;
        }
    }
    bool isItalic() const { return italic_; }

    // 是否允许换行（由上层根据是否行内元素设置）
    void setWrap(bool wrap) { 
        if (wrap_ != wrap) {
            wrap_ = wrap; 
            layoutDirty_ = true;
        }
    }
    bool getWrap() const { return wrap_; }

    // 文本行高
    void setLineHeight(float lineHeight) {
        if (lineHeight_ != lineHeight) {
            lineHeight_ = lineHeight;
            layoutDirty_ = true;
        }
    }
    float getLineHeight() const { return lineHeight_; }

    // 文本对齐
    enum class Alignment {
        Left,
        Center,
        Right
    };

    void setAlignment(Alignment align) { 
        if (alignment_ != align) {
            alignment_ = align; 
            layoutDirty_ = true;
        }
    }
    Alignment getAlignment() const { return alignment_; }

    void setAlignment(ldt::TextAlign align) {
        Alignment newAlign = Alignment::Left;
        if (align == ldt::TextAlign::Center) newAlign = Alignment::Center;
        else if (align == ldt::TextAlign::Right) newAlign = Alignment::Right;
        
        if (alignment_ != newAlign) {
            alignment_ = newAlign;
            layoutDirty_ = true;
        }
    }

    // Accept string alignment values ("left", "center", "right") for convenience
    void setAlignment(const std::string& s) {
        std::string t = s;
        for (auto &c : t) c = static_cast<char>(::tolower(c));
        Alignment newAlign = Alignment::Left;
        if (t == "center" || t == "middle") newAlign = Alignment::Center;
        else if (t == "right") newAlign = Alignment::Right;
        
        if (alignment_ != newAlign) {
            alignment_ = newAlign;
            layoutDirty_ = true;
        }
    }

    // 文本颜色
    void setTextColor(const ui::Color& color) { textColor_ = color; }
    const ui::Color& getTextColor() const { return textColor_; }

    // 布局宽度（用于对齐，不影响 bounds_）
    void setLayoutWidth(float width) { layoutWidth_ = width; }
    float getLayoutWidth() const { return layoutWidth_; }

private:
    std::string text_;
    std::string fontFamily_;
    float fontSize_ = 0.0f;  // 0表示使用默认大小
    float lineHeight_ = 0.0f;
    bool bold_ = false;
    bool italic_ = false;
    Alignment alignment_ = Alignment::Left;
    ui::Color textColor_{0.0f, 0.0f, 0.0f, 0.0f};
    bool wrap_ = true;
    float layoutWidth_ = 0.0f;

    mutable std::shared_ptr<ITextLayout> textLayout_;
    mutable bool layoutDirty_ = true;
    mutable float lastLayoutWidth_ = 0.0f;

    void updateTextLayout() {
        if (!getScene()) return;
        auto measurer = getScene()->getTextMeasurer();
        if (!measurer) return;
        
        IDrawer* drawer = dynamic_cast<IDrawer*>(measurer);
        if (!drawer) return;

        float fontSize = fontSize_ > 0.0f ? fontSize_ : 14.0f;
        Font font;
        font.family = fontFamily_;
        font.size = fontSize;
        font.bold = bold_;
        font.italic = italic_;

        TextLayoutParams params;
        switch (alignment_) {
            case Alignment::Left: params.textAlign = "left"; break;
            case Alignment::Center: params.textAlign = "center"; break;
            case Alignment::Right: params.textAlign = "right"; break;
            default: params.textAlign = "left"; break;
        }
        params.wrap = wrap_;
        params.lineHeight = lineHeight_;

        float innerW = bounds_.width - (paddingLeft_ + paddingRight_);
        
        float maxWidth = -1.0f;
        if (wrap_) {
            maxWidth = innerW;
            lastLayoutWidth_ = innerW;
        } else {
            lastLayoutWidth_ = -1.0f;
        }

        textLayout_ = drawer->createTextLayout(text_, font, params, maxWidth);
        layoutDirty_ = false;
    }
};

} // namespace ldt
