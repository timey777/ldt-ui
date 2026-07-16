#pragma once
#include "abstract_control.h"
#include "render/display_list.h"
#include "scene.h"
#include "render/text_measurer.h"
#include "engine/ui_event_system.h"
#include "render/drawer.h"
#include "control_factory.h"
#include "render/text_layout.h"
#include "misc/utf8util.h"
#include "engine/update_scheduler.h"
#include <string>
#include <vector>
#include <chrono>
#include "engine/input_events.h"
#include <limits>
#include <cmath>
#include <algorithm>
#include <cassert>
namespace ldt {
class LDT_API Input : public AbstractControl {
public:
    Input() {
        ensureScrollbarCreated();
    }
    explicit Input(const std::string& value) : value_(value) {
        ensureScrollbarCreated();
    }

public:
    void onRender(DisplayList& displayList, const ui::Rect& clip) const override {
        struct RenderPhaseScope {
            explicit RenderPhaseScope(const Input* owner) {
                previous_ = Input::activeRenderInput();
                Input::activeRenderInput() = owner;
            }
            ~RenderPhaseScope() {
                Input::activeRenderInput() = previous_;
            }
            const Input* previous_ = nullptr;
        } renderScope(this);

        // 绘制输入框背景
        displayList.addRect(bounds_, fillColor_, strokeColor_, strokeWidth_, cornerRadius_);
        
        // 绘制输入框内容（value 或 placeholder）
        ui::Color textColor = textColor_.a > 0.0f ? textColor_ : ui::Color(0.0f, 0.0f, 0.0f, 1.0f);
        float fontSize = fontSize_ > 0.0f ? fontSize_ : 14.0f; // fallback

        const TextRenderGeometry geometry = getTextRenderGeometry(fontSize);
        bool layoutReady = (!layoutDirty_ && textLayout_ != nullptr);
        
        // 绘制选区背景（若有选区）
        // Push clip so selection/text/caret are clipped to content area
        displayList.pushClip(geometry.contentClipRect);
        // 滚动只做平移，不触发布局重建（垂直 + 水平）
        displayList.pushTransform(-geometry.currentScrollX, -geometry.currentScrollY);

        // 绘制整行边框（仅多行文本类型且 textLayout_ 存在时绘制）
        if (multiline_ && layoutReady) {
            // 获取光标位置的矩形来确定当前行位置
            auto caretR = textLayout_->getCursorRect(caretPos_);
            
            float lw = 1.0f;
            ui::Rect lineRect(
                geometry.textOriginX + lw * 0.5f,
                geometry.textOriginY + caretR.y + lw * 0.5f,
                geometry.innerWidth - lw,
                caretR.h - lw);
            
            displayList.addRect(lineRect, ui::Color(0,0,0,0), lineBorderColor_, lw);
        }

        if (layoutReady && hasSelection() && !value_.empty()) {
            for (const auto& r : selectionRects_) {
                displayList.addRect(
                    ui::Rect(geometry.textOriginX + r.x, geometry.textOriginY + r.y, r.w, r.h),
                    ui::Color(0.2f, 0.45f, 0.95f, 0.35f));
            }
        }
        
        // 文本渲染
        if (value_.empty() && !placeholder_.empty()) {
            // value 为空时渲染 placeholder，使用半透明灰色，不参与光标/选区交互
            ui::Color placeholderColor = textColor;
            placeholderColor.a *= 0.4f;
            TextLayoutParams placeholderParams;
            placeholderParams.wrap = false;
            placeholderParams.textAlign = "left";
            placeholderParams.lineHeight = geometry.lineHeight;
            displayList.addText(
                placeholder_,
                ui::Rect(
                    geometry.textOriginX,
                    geometry.textOriginY,
                    geometry.innerWidth,
                    geometry.lineHeight),
                placeholderColor,
                fontSize,
                fontFamily_.empty() ? "" : fontFamily_,
                placeholderParams
            );
        } else if (layoutReady && !value_.empty()) {
            if (multiline_ && wrap_) {
                // 自动换行模式：使用 textLayout_ 渲染（内部处理 \n + 宽度换行，显示换行不改字符串）
                float layoutWidth = std::max(geometry.innerWidth,
                    textLayout_->getBounds().w);
                displayList.addTextLayout(
                    textLayout_,
                    ui::Rect(geometry.textOriginX, geometry.textOriginY,
                             layoutWidth, textLayout_->getBounds().h),
                    textColor);
            } else if (!lines_.empty()) {
                // no-wrap 模式：按 \n 分隔的逐行渲染 + 水平滚动
                float lineRenderWidth = (multiline_ && !wrap_)
                    ? std::max(geometry.innerWidth, totalContentWidth_ - paddingLeft_ - paddingRight_)
                    : geometry.innerWidth;

                int firstLine = std::max(0, static_cast<int>(geometry.currentScrollY / std::max(1.0f, geometry.lineHeight)));
                int visibleLineCount = std::max(1, static_cast<int>(geometry.contentClipRect.height / std::max(1.0f, geometry.lineHeight)) + 2);
                int endLine = std::min(static_cast<int>(lines_.size()), firstLine + visibleLineCount);

                TextLayoutParams lineParams;
                lineParams.wrap = false;
                lineParams.textAlign = "left";
                lineParams.lineHeight = geometry.lineHeight;

                for (int i = firstLine; i < endLine; ++i) {
                    const auto& info = lines_[static_cast<size_t>(i)];
                    std::string lineText = value_.substr(info.startOffset, info.length);
                    displayList.addText(
                        lineText,
                        ui::Rect(
                            geometry.textOriginX,
                            geometry.textOriginY + static_cast<float>(i) * geometry.lineHeight,
                            lineRenderWidth,
                            geometry.lineHeight),
                        textColor,
                        fontSize,
                        fontFamily_.empty() ? "" : fontFamily_,
                        lineParams
                    );
                }
            }
        }

        // 绘制光标（由 update 控制可见性）
        if (isFocused() && caretVisible_ && layoutReady) {
            auto caretR = textLayout_->getCursorRect(caretPos_);
            displayList.addRect(
                ui::Rect(geometry.textOriginX + caretR.x, geometry.textOriginY + caretR.y, caretR.w, caretR.h),
                caretColor_);
        }

        // 恢复 transform
        displayList.popTransform();
        // pop clip for content area
        displayList.popClip();

        // 滚动条生命周期固定，只切换可见性
        if (vScrollbar_ && geometry.maxScrollY > 0.0f) {
            vScrollbar_->render(displayList, clip);
        }
        if (hScrollbar_ && geometry.maxScrollX > 0.0f) {
            hScrollbar_->render(displayList, clip);
        }
    }

    std::string getTypeName() const override { return "Input"; }

    void setValue(const std::string& v, bool notify = true) { 
        if (value_ != v) {
            value_ = v; 
            requestLayoutRebuild();
            preferredCursorX_ = -1.0f;
            if (layoutDirty_) updateTextLayout();
            invalidate(); 
            if (notify) emitInputEvent(); 
        }
    }
    const std::string& getValue() const { return value_; }

    void setPlaceholder(const std::string& p) { 
        if (placeholder_ != p) {
            placeholder_ = p; 
            if (value_.empty()) {
                requestLayoutRebuild();
                if (layoutDirty_) updateTextLayout();
            }
            invalidate();
        }
    }
    const std::string& getPlaceholder() const { return placeholder_; }

    void setType(const std::string& t) { 
        if (type_ != t) {
            type_ = t; 
            multiline_ = (type_ == "textarea");
            if (multiline_) wrap_ = true;  // 默认：textarea 自动换行
            requestLayoutRebuild();
            if (layoutDirty_) updateTextLayout();
            invalidate();
        }
    }
    const std::string& getType() const { return type_; }

    void setWrap(bool wrap) {
        if (wrap_ != wrap) {
            wrap_ = wrap;
            requestLayoutRebuild();
            if (layoutDirty_) updateTextLayout();
            invalidate();
        }
    }
    bool getWrap() const { return wrap_; }

    // 文本颜色
    void setTextColor(const ui::Color& color) {
        textColor_ = color; 
        updateLineBorderColor();
    }
    const ui::Color& getTextColor() const { return textColor_; }

    void setFillColor(const ui::Color& color) override {
        AbstractControl::setFillColor(color);
        updateCaretColor();
    }

    void setFontSize(float size) { 
        if (fontSize_ != size) {
            fontSize_ = size; 
            requestLayoutRebuild();
            if (layoutDirty_) updateTextLayout();
            invalidate();
        }
    }
    float getFontSize() const { return fontSize_; }

    void setFontFamily(const std::string& family) { 
        if (fontFamily_ != family) {
            fontFamily_ = family; 
            requestLayoutRebuild();
            if (layoutDirty_) updateTextLayout();
            invalidate();
        }
    }
    const std::string& getFontFamily() const { return fontFamily_; }
    
    void setPadding(float left, float top, float right, float bottom) override {
        AbstractControl::setPadding(left, top, right, bottom);
        requestLayoutRebuild();
        if (layoutDirty_) updateTextLayout();
        invalidate();
    }

    void setScene(Scene* scene) override {
        AbstractControl::setScene(scene);
        if (vScrollbar_) vScrollbar_->setScene(scene);
    }

    // Override setScrollSize to ignore updates from layout engine (Compositor)
    // Input computes and applies scroll size inside updateTextLayout().
    void setScrollSize(float w, float h) override {
        (void)w;
        (void)h;
        // Do nothing.
    }
    
private:
    struct TextLayoutLocalPoint {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct TextRenderGeometry {
        float innerWidth = 0.0f;
        float lineHeight = 0.0f;
        ui::Rect contentClipRect;
        float contentHeight = 0.0f;
        float totalContentHeight = 0.0f;
        float maxScrollY = 0.0f;
        float currentScrollY = 0.0f;
        float maxScrollX = 0.0f;
        float currentScrollX = 0.0f;
        float textOriginX = 0.0f;
        float textOriginY = 0.0f;
    };

    struct LineInfo {
        size_t startOffset = 0;
        size_t length = 0;
    };

    std::string value_;
    std::string placeholder_;
    std::string type_ = "Input";
    bool multiline_ = false;
    bool wrap_ = false;  // 多行时是否自动换行（显示换行，不改变 value_ 字符串）
    ui::Color textColor_{0.0f, 0.0f, 0.0f, 0.0f};
    ui::Color caretColor_{0,0,0,1};
    ui::Color lineBorderColor_{0,0,0,0.2f};
    std::string fontFamily_;
    float fontSize_ = 0.0f;  // 0表示使用默认大小
    // padding (visual inset for text drawing)
    // 编辑相关状态
    int caretPos_ = 0; // 插入点偏移（codepoint 索引）
    // 光标可见性由 update() 管理，render 不做时间判断
    bool caretVisible_ = false;
    std::chrono::steady_clock::time_point lastBlinkTime_ = std::chrono::steady_clock::now();
    // 选择相关
    int selectionStart_ = -1; // 如果 selectionStart_==-1 表示无选择
    int selectionEnd_ = -1;
    bool selecting_ = false; // 鼠标拖拽中

    // 长按退格/删除支持
    bool backspaceHeld_ = false;
    bool deleteHeld_ = false;
    // 计时器（毫秒）
    int backspaceTimerMs_ = 0;
    const int backspaceInitialDelayMs_ = 400;
    const int backspaceRepeatIntervalMs_ = 50;

    // overlay scrollbar instances (created lazily). mutable because render() is const.
    mutable std::shared_ptr<ScrollbarControl> vScrollbar_;
    mutable std::shared_ptr<ScrollbarControl> hScrollbar_;

    // Layout cache
    mutable std::shared_ptr<ITextLayout> textLayout_;
    mutable bool layoutDirty_ = true;
    float lastLayoutInnerWidth_ = -1.0f;
    float contentHeight_ = 0.0f;
    float totalContentHeight_ = 0.0f;
    float totalContentWidth_ = 0.0f;
    float lineHeight_ = 0.0f;
    std::vector<LineInfo> lines_;
    std::vector<render::Rect> selectionRects_;
    bool selectionDirty_ = true;
    // Remember desired X position when moving vertically
    float preferredCursorX_ = -1.0f;

    float getInnerTextWidth() const {
        return bounds_.width - (paddingLeft_ + paddingRight_);
    }

    float getResolvedLineHeight(float fontSize) const {
        return lineHeight_ > 0.0f ? lineHeight_ : fontSize * 1.2f;
    }

    float getTextLocalStartX() const {
        return paddingLeft_;
    }

    float getTextLocalStartY() const {
        float startY = paddingTop_;
        if (!multiline_) {
            float availableHeight = bounds_.height - (paddingTop_ + paddingBottom_);
            float contentHeight = contentHeight_;
            if (availableHeight > contentHeight) {
                startY += (availableHeight - contentHeight) * 0.5f;
            }
        }
        return startY;
    }

    TextRenderGeometry getTextRenderGeometry(float fontSize) const {
        const float innerWidth = getInnerTextWidth();
        const float lineHeight = getResolvedLineHeight(fontSize);
        const float contentHeight = contentHeight_ > 0.0f ? contentHeight_ : lineHeight;
        const float totalContentHeight = totalContentHeight_ > 0.0f
            ? totalContentHeight_
            : (contentHeight + paddingTop_ + paddingBottom_);
        const float totalContentWidth = totalContentWidth_ > 0.0f
            ? totalContentWidth_
            : innerWidth;

        return {
            innerWidth,
            lineHeight,
            ui::Rect(
                bounds_.x + getTextLocalStartX(),
                bounds_.y + paddingTop_,
                innerWidth,
                bounds_.height - (paddingTop_ + paddingBottom_)),
            contentHeight,
            totalContentHeight,
            std::max(0.0f, totalContentHeight - bounds_.height),   // maxScrollY
            getScrollY(),
            std::max(0.0f, totalContentWidth - innerWidth),         // maxScrollX
            getScrollX(),
            bounds_.x + getTextLocalStartX(),
            bounds_.y + getTextLocalStartY()
        };
    }

    TextLayoutLocalPoint toTextLayoutLocalPoint(ControlLocalPointDp point) const {
        return {
            point.x.value - getTextLocalStartX() + getScrollX(),
            point.y.value - getTextLocalStartY() + getScrollY()
        };
    }

    int getCharIndexAtControlPoint(ControlLocalPointDp point) const {
        if (!textLayout_) {
            return 0;
        }

        const TextLayoutLocalPoint textPoint = toTextLayoutLocalPoint(point);
        return textLayout_->getCharIndexAt(textPoint.x, textPoint.y);
    }

    void updateCaretColor() {
        if (fillColor_.a > 0.1f) {
            float lum = 0.299f * fillColor_.r + 0.587f * fillColor_.g + 0.114f * fillColor_.b;
            if (lum < 0.5f) caretColor_ = ui::Color(1,1,1,1);
            else caretColor_ = ui::Color(0,0,0,1);
        } else {
             caretColor_ = ui::Color(0,0,0,1);
        }
    }

    void updateLineBorderColor() {
        ui::Color borderColor = textColor_;
        if (borderColor.a == 0.0f) borderColor = ui::Color(0,0,0,1);
        borderColor.a = 0.2f; // 20% opacity
        lineBorderColor_ = borderColor;
    }

    void ensureScrollbarCreated() {
        if (!vScrollbar_) {
            vScrollbar_ = ControlFactory::getInstance()->CreateScrollbar(ScrollbarControl::Orientation::Vertical);
            vScrollbar_->setParent(this);
            vScrollbar_->setVisible(false);
        }
        if (!hScrollbar_) {
            hScrollbar_ = ControlFactory::getInstance()->CreateScrollbar(ScrollbarControl::Orientation::Horizontal);
            hScrollbar_->setParent(this);
            hScrollbar_->setVisible(false);
        }
    }

    void updateScrollbarVisibility() {
        ensureScrollbarCreated();
        if (vScrollbar_) {
            float maxScrollY = std::max(0.0f, totalContentHeight_ - bounds_.height);
            vScrollbar_->setVisible(maxScrollY > 0.0f);
        }
        if (hScrollbar_) {
            float innerW = getInnerTextWidth();
            float maxScrollX = std::max(0.0f, totalContentWidth_ - innerW);
            hScrollbar_->setVisible(maxScrollX > 0.0f);
        }
    }

    void requestLayoutRebuild() {
        assert(!isInRenderPhaseChangingLayout());
        layoutDirty_ = true;
        selectionDirty_ = true;
    }

    bool isInRenderPhaseChangingLayout() const {
        return activeRenderInput() == this;
    }

    static const Input*& activeRenderInput() {
        static thread_local const Input* active = nullptr;
        return active;
    }

    void preprocessLines(const std::string& text) {
        lines_.clear();
        if (text.empty()) {
            lines_.push_back(LineInfo{0, 0});
            return;
        }

        size_t start = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\n') {
                lines_.push_back(LineInfo{start, i - start});
                start = i + 1;
            }
        }
        lines_.push_back(LineInfo{start, text.size() - start});
    }

    void refreshSelectionRectsCache() {
        if (!selectionDirty_) return;
        selectionRects_.clear();
        if (!hasSelection() || value_.empty()) {
            selectionDirty_ = false;
            return;
        }

        if (!textLayout_ || layoutDirty_) {
            return;
        }

        selectionRects_ = textLayout_->getSelectionRects(selectionStart_, selectionEnd_);
        selectionDirty_ = false;
    }

    void setSelectionRange(int start, int end) {
        if (selectionStart_ == start && selectionEnd_ == end) return;
        selectionStart_ = start;
        selectionEnd_ = end;
        selectionDirty_ = true;
        refreshSelectionRectsCache();
    }

    void clearSelection() {
        setSelectionRange(-1, -1);
    }

public:
    void updateTextLayout() {
        assert(!isInRenderPhaseChangingLayout());
        float innerW = getInnerTextWidth();
        if (std::fabs(innerW - lastLayoutInnerWidth_) > 0.1f) {
            layoutDirty_ = true;
        }

        if (!layoutDirty_ && textLayout_) {
            float scrollW = (multiline_ && !wrap_) ? totalContentWidth_ : innerW;
            AbstractControl::setScrollSize(scrollW, totalContentHeight_);
            updateScrollbarVisibility();
            refreshSelectionRectsCache();
            return;
        }

        if (!getScene()) return;
        auto measurer = getScene()->getTextMeasurer();
        if (!measurer) return;
        
        // Try to get IDrawer from measurer (assuming they are the same object)
        IDrawer* drawer = dynamic_cast<IDrawer*>(measurer);
        if (!drawer) return;

        float fontSize = fontSize_ > 0.0f ? fontSize_ : 14.0f;
        Font font;
        font.family = fontFamily_.empty() ? "" : fontFamily_;
        font.size = fontSize;

        TextLayoutParams params;
        params.wrap = (multiline_ && wrap_);  // 多行 + 自动换行时才 wrap
        params.textAlign = "left";
        params.lineHeight = getResolvedLineHeight(fontSize);
        lineHeight_ = params.lineHeight;

        float maxWidth = -1.0f;
        if (multiline_ && wrap_) {
            maxWidth = innerW;  // 自动换行：限制宽度
        }
        // no-wrap 时 maxWidth = -1（不限制），由 textLayout 给出真实宽度

        // 始终基于真实 value_ 构建 text layout，placeholder 仅用于渲染显示，
        // 不应影响光标定位、点击和选区逻辑
        preprocessLines(value_);
        
        textLayout_ = drawer->createTextLayout(value_, font, params, maxWidth);
        float textWidth = textLayout_ ? textLayout_->getBounds().w : 0.0f;
        contentHeight_ = textLayout_ ? textLayout_->getBounds().h : 0.0f;
        float minContentHeight = std::max(lineHeight_, static_cast<float>(lines_.size()) * lineHeight_);
        if (contentHeight_ < minContentHeight) contentHeight_ = minContentHeight;
        totalContentHeight_ = contentHeight_ + paddingTop_ + paddingBottom_;

        // 水平内容宽度：wrap 模式 = innerW，no-wrap 模式 = 实际文本宽度 + padding
        if (multiline_ && !wrap_) {
            totalContentWidth_ = textWidth + paddingLeft_ + paddingRight_;
        } else {
            totalContentWidth_ = innerW;
        }

        float scrollW = (multiline_ && !wrap_) ? totalContentWidth_ : innerW;
        AbstractControl::setScrollSize(scrollW, totalContentHeight_);
        updateScrollbarVisibility();

        lastLayoutInnerWidth_ = innerW;
        layoutDirty_ = false;
        refreshSelectionRectsCache();
    }

    // UTF-8 helpers: map between codepoint index and byte offsets in value_
    int codepointCount() const {
        std::vector<int> idx;
        auto cps = utf8_to_utf32_with_offsets(value_, idx);
        return (int)idx.size();
    }

    int codepointIndexToByteIndex(int cpIndex) const {
        if (cpIndex <= 0) return 0;
        std::vector<int> idx;
        auto cps = utf8_to_utf32_with_offsets(value_, idx);
        if (cpIndex >= (int)idx.size()) return (int)value_.size();
        return idx[cpIndex];
    }

    int byteIndexToCodepointIndex(int byteIndex) const {
        if (byteIndex <= 0) return 0;
        std::vector<int> idx;
        auto cps = utf8_to_utf32_with_offsets(value_, idx);
        auto it = std::lower_bound(idx.begin(), idx.end(), byteIndex);
        return (int)(it - idx.begin());
    }

    void deletePreviousCodepoint() {
        std::vector<int> idx;
        auto cps = utf8_to_utf32_with_offsets(value_, idx);
        if (caretPos_ <= 0) return;
        int prev = caretPos_ - 1;
        if (prev < 0) return;
        int start = idx[prev];
        int end = (prev + 1 < (int)idx.size()) ? idx[prev+1] : (int)value_.size();
        value_.erase((size_t)start, (size_t)(end - start));
        caretPos_ = prev;
        requestLayoutRebuild();
        updateTextLayout();
        emitInputEvent();
    }

    void deleteNextCodepoint() {
        std::vector<int> idx;
        auto cps = utf8_to_utf32_with_offsets(value_, idx);
        if (caretPos_ < (int)idx.size()) {
            int start = idx[caretPos_];
            int end = (caretPos_ + 1 < (int)idx.size()) ? idx[caretPos_+1] : (int)value_.size();
            value_.erase((size_t)start, (size_t)(end - start));
            requestLayoutRebuild();
            updateTextLayout();
            emitInputEvent();
        }
    }

    // 事件处理
    void onFocus() override {
        AbstractControl::onFocus();
        // show caret immediately and schedule blinking
        caretVisible_ = true;
        lastBlinkTime_ = std::chrono::steady_clock::now();
        invalidate();
    }
    void onBlur() override {
        AbstractControl::onBlur();
        // hide caret and request redraw
        caretVisible_ = false;
        // clear selection on blur
        clearSelection();
        invalidate();
        // 不在控件内部写回 AST：由宿主/Scene 在热重载前统一同步运行时值
    }

    // 每帧更新（由 Scene 调度）
    void update() override {
        using namespace std::chrono;
        auto now = steady_clock::now();
        bool keepAnimating = false;

        // Blink handling only when focused
        if (isFocused()) {
            keepAnimating = true;
            auto elapsed = duration_cast<milliseconds>(now - lastBlinkTime_).count();
            if (elapsed >= 500) {
                lastBlinkTime_ = now;
                caretVisible_ = !caretVisible_;
            }
        }

        // Handle long-press backspace/delete repeating
        if (backspaceHeld_ || deleteHeld_) {
            keepAnimating = true;
            backspaceTimerMs_ += 16; // approx per-frame
            if (backspaceTimerMs_ >= backspaceInitialDelayMs_) {
                if ((backspaceTimerMs_ - backspaceInitialDelayMs_) % backspaceRepeatIntervalMs_ < 16) {
                    if (backspaceHeld_) {
                        if (hasSelection()) {
                            deleteSelection();
                        } else if (caretPos_ > 0 && !value_.empty()) {
                            deletePreviousCodepoint();
                            preferredCursorX_ = -1.0f;
                        }
                    }
                    if (deleteHeld_) {
                        if (hasSelection()) {
                            deleteSelection();
                        } else {
                            deleteNextCodepoint();
                            preferredCursorX_ = -1.0f;
                        }
                    }
                }
            }
        } else {
            backspaceTimerMs_ = 0;
        }

        if (keepAnimating) {
            invalidate();
        }
    }

    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override {
        if (!isVisible()) return false;
        const float x = point.x.value;
        const float y = point.y.value;
        
        // Ensure layout is up to date for hit testing
        if (layoutDirty_ || !textLayout_) {
            updateTextLayout();
        }

        // Scrollbar handling
        if (vScrollbar_ && vScrollbar_->isVisible() && vScrollbar_->onParentLocalMouseButton(makeParentLocalPoint(x, y), button, action)) return true;
        if (hScrollbar_ && hScrollbar_->isVisible() && hScrollbar_->onParentLocalMouseButton(makeParentLocalPoint(x, y), button, action)) return true;
        
        if (action == GLFW_PRESS) { // press
            if (!hitTestLocal(point)) return false;
            
            int pos = 0;
            if (textLayout_) {
                pos = getCharIndexAtControlPoint(makeControlLocalPoint(x, y));
            }
            
            // Set caret and begin selection on press
            caretPos_ = pos; caretVisible_ = true;
            preferredCursorX_ = -1.0f; // Reset preferred X on click
            // start selecting state
            selecting_ = true;
            setSelectionRange(caretPos_, caretPos_);
            
            captureMouse();
            invalidate();
            return true;
        }
        if (action == GLFW_RELEASE) {
            // stop drag-select
            if (selecting_)
            {
                selecting_ = false;
                releaseMouse();
            }
            return true;
        }
        return false;
    }

    // 鼠标移动用于拖拽选择
    bool handleLocalMouseMove(ControlLocalPointDp point) override {
        if (!isVisible()) return false;
        const float x = point.x.value;
        const float y = point.y.value;
        // First forward to scrollbars if present
        if (vScrollbar_ && vScrollbar_->isVisible() && vScrollbar_->onParentLocalMouseMove(makeParentLocalPoint(x, y))) return true;
        if (hScrollbar_ && hScrollbar_->onParentLocalMouseMove(makeParentLocalPoint(x, y))) return true;

        // set I-beam cursor when hovering over input area
        if (hitTestLocal(point) && getScene()) getScene()->setCursor(ldt::Scene::CursorType::Cursor_IBeam);

        if (!selecting_) return false;
        
        if (layoutDirty_ || !textLayout_) {
            updateTextLayout();
        }

        int pos = 0;
        if (textLayout_) {
            pos = getCharIndexAtControlPoint(makeControlLocalPoint(x, y));
        }
        
        caretPos_ = pos;
        setSelectionRange(selectionStart_, caretPos_);
        invalidate();
        return true;
    }

    bool onKey(int key, int scancode, int action, int mods) override {
        if (!isFocused()) return false;

        // Ctrl+A -> select all
        if (key == GLFW_KEY_A && (mods & GLFW_MOD_CONTROL)) {
            setSelectionRange(0, codepointCount());
            caretPos_ = selectionEnd_;
            invalidate();
            return true;
        }

        // Ctrl+C -> copy selection or full value
        if (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
            std::string toCopy;
            if (hasSelection()) {
                int a = std::min(selectionStart_, selectionEnd_);
                int b = std::max(selectionStart_, selectionEnd_);
                if (a < 0) a = 0;
                // convert codepoint indices to byte offsets
                std::vector<int> idx;
                auto cps = utf8_to_utf32_with_offsets(value_, idx);
                int aByte = (a < (int)idx.size()) ? idx[a] : (int)value_.size();
                int bByte = (b < (int)idx.size()) ? idx[b] : (int)value_.size();
                toCopy = value_.substr((size_t)aByte, (size_t)(bByte - aByte));
            } else {
                toCopy = value_;
            }
            if (!toCopy.empty()) {
                if (getScene()) getScene()->setClipboardString(toCopy);
            }
            return true;
        }

        // Ctrl+V -> paste clipboard (replace selection if present)
        if (key == GLFW_KEY_V && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
            if (getScene()) {
                std::string clip = getScene()->getClipboardString();
                if (!clip.empty()) {
                    if (hasSelection()) {
                        deleteSelection();
                    }
                    int insertByte = codepointIndexToByteIndex(caretPos_);
                    value_.insert((size_t)insertByte, clip);
                    // advance caret by number of codepoints in clip
                    std::vector<int> clipIdx;
                    auto clipCps = utf8_to_utf32_with_offsets(clip, clipIdx);
                    caretPos_ = static_cast<int>(caretPos_ + (int)clipIdx.size());
                    clearSelection();
                    requestLayoutRebuild();
                    updateTextLayout();
                    invalidate();
                    emitInputEvent();
                }
            }
            return true;
        }

        // Ctrl+X -> cut selection
        if (key == GLFW_KEY_X && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
            if (hasSelection()) {
                // copy selection to clipboard then delete
                std::string toCopy;
                int a = std::min(selectionStart_, selectionEnd_);
                int b = std::max(selectionStart_, selectionEnd_);
                if (a < 0) a = 0;
                std::vector<int> idx;
                auto cps = utf8_to_utf32_with_offsets(value_, idx);
                int aByte = (a < (int)idx.size()) ? idx[a] : (int)value_.size();
                int bByte = (b < (int)idx.size()) ? idx[b] : (int)value_.size();
                toCopy = value_.substr((size_t)aByte, (size_t)(bByte - aByte));
                if (!toCopy.empty() && getScene()) getScene()->setClipboardString(toCopy);
                deleteSelection();
                invalidate();
            }
            return true;
        }

        // Enter -> 在多行输入中插入换行
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
            if (multiline_) {
                if (hasSelection()) deleteSelection();
                int insertByte = codepointIndexToByteIndex(caretPos_);
                value_.insert((size_t)insertByte, "\n");
                caretPos_ = static_cast<int>(caretPos_ + 1);
                clearSelection();
                requestLayoutRebuild();
                updateTextLayout();
                invalidate();
                emitInputEvent();
                return true;
            }
        }

        // Handle key press/release for long-press behavior
        if (key == GLFW_KEY_BACKSPACE) {
            if (action == GLFW_PRESS) {
                // if selection exists, delete it immediately
                if (hasSelection()) {
                    deleteSelection();
                    invalidate();
                } else if (caretPos_ > 0 && !value_.empty()) {
                    preferredCursorX_ = -1.0f;
                    deletePreviousCodepoint();
                    invalidate();
                }
                backspaceHeld_ = true;
                backspaceTimerMs_ = 0;
                return true;
            } else if (action == GLFW_RELEASE) {
                backspaceHeld_ = false;
                backspaceTimerMs_ = 0;
                return true;
            }
        }

        if (key == GLFW_KEY_DELETE) {
            if (action == GLFW_PRESS) {
                if (hasSelection()) {
                    deleteSelection();
                    preferredCursorX_ = -1.0f;
                    invalidate();
                } else {
                    deleteNextCodepoint();
                    invalidate();
                }
                deleteHeld_ = true;
                backspaceTimerMs_ = 0;
                return true;
            } else if (action == GLFW_RELEASE) {
                deleteHeld_ = false;
                backspaceTimerMs_ = 0;
                return true;
            }
        }

        // Arrow navigation (press or repeat)
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            if (key == GLFW_KEY_LEFT) {
                if (caretPos_ > 0) caretPos_--;
                // collapse selection on arrow
                clearSelection();
                preferredCursorX_ = -1.0f;
                invalidate();
                return true;
            } else if (key == GLFW_KEY_RIGHT) {
                if (caretPos_ < codepointCount()) caretPos_++;
                clearSelection();
                preferredCursorX_ = -1.0f;
                invalidate();
                return true;
            } else if (key == GLFW_KEY_UP) {
                if (textLayout_) {
                    auto r = textLayout_->getCursorRect(caretPos_);
                    if (preferredCursorX_ < 0.0f) preferredCursorX_ = r.x;
                    
                    // Move up approx 1 line
                    float targetY = r.y - r.h * 0.5f; 
                    caretPos_ = textLayout_->getCharIndexAt(preferredCursorX_, targetY);
                    clearSelection();
                    invalidate();
                }
                return true;
            } else if (key == GLFW_KEY_DOWN) {
                if (textLayout_) {
                    auto r = textLayout_->getCursorRect(caretPos_);
                    if (preferredCursorX_ < 0.0f) preferredCursorX_ = r.x;

                    // Move down approx 1 line
                    float targetY = r.y + r.h * 1.5f;
                    caretPos_ = textLayout_->getCharIndexAt(preferredCursorX_, targetY);
                    clearSelection();
                    invalidate();
                }
                return true;
            } else if (key == GLFW_KEY_HOME) {
                if (!multiline_) {
                    caretPos_ = 0;
                    clearSelection();
                    preferredCursorX_ = -1.0f;
                    invalidate();
                    return true;
                }
                if (textLayout_) {
                    auto r = textLayout_->getCursorRect(caretPos_);
                    float targetY = r.y + r.h * 0.5f;
                    // move to start of line
                    caretPos_ = textLayout_->getCharIndexAt(0.0f, targetY);
                    clearSelection();
                    preferredCursorX_ = -1.0f;
                    // ensure caret visible
                    float viewportH = bounds_.height - (paddingTop_ + paddingBottom_);
                    float maxScrollY = std::max(0.0f, getScrollHeight() - viewportH);
                    auto cr = textLayout_->getCursorRect(caretPos_);
                    float newY = getScrollY();
                    if (cr.y < newY) newY = cr.y;
                    if (cr.y + cr.h > newY + viewportH) newY = cr.y + cr.h - viewportH;
                    newY = std::max(0.0f, std::min(newY, maxScrollY));
                    if (newY != getScrollY()) { setScroll(getScrollX(), newY); markLayerDirty(); if (getScene()) UpdateScheduler::getInstance().requestPaint(); }
                    invalidate();
                }
                return true;
            } else if (key == GLFW_KEY_END) {
                if (!multiline_) {
                    caretPos_ = codepointCount();
                    clearSelection();
                    preferredCursorX_ = -1.0f;
                    invalidate();
                    return true;
                }
                if (textLayout_) {
                    auto r = textLayout_->getCursorRect(caretPos_);
                    float targetY = r.y + r.h * 0.5f;
                    // move to end of line (use large X to get rightmost)
                    caretPos_ = textLayout_->getCharIndexAt(1e6f, targetY);
                    clearSelection();
                    preferredCursorX_ = -1.0f;
                    // ensure caret visible
                    float viewportH = bounds_.height - (paddingTop_ + paddingBottom_);
                    float maxScrollY = std::max(0.0f, getScrollHeight() - viewportH);
                    auto cr = textLayout_->getCursorRect(caretPos_);
                    float newY = getScrollY();
                    if (cr.y < newY) newY = cr.y;
                    if (cr.y + cr.h > newY + viewportH) newY = cr.y + cr.h - viewportH;
                    newY = std::max(0.0f, std::min(newY, maxScrollY));
                    if (newY != getScrollY()) { setScroll(getScrollX(), newY); markLayerDirty(); if (getScene()) UpdateScheduler::getInstance().requestPaint(); }
                    invalidate();
                }
                return true;
            } else if (key == GLFW_KEY_PAGE_UP) {
                if (textLayout_) {
                    auto r = textLayout_->getCursorRect(caretPos_);
                    if (preferredCursorX_ < 0.0f) preferredCursorX_ = r.x;
                    float viewportH = bounds_.height - (paddingTop_ + paddingBottom_);
                    float targetY = r.y - viewportH;
                    caretPos_ = textLayout_->getCharIndexAt(preferredCursorX_ < 0.0f ? r.x : preferredCursorX_, targetY);
                    clearSelection();
                    // adjust scroll
                    float maxScrollY = std::max(0.0f, getScrollHeight() - viewportH);
                    auto cr = textLayout_->getCursorRect(caretPos_);
                    float newY = getScrollY();
                    if (cr.y < newY) newY = cr.y;
                    if (cr.y + cr.h > newY + viewportH) newY = cr.y + cr.h - viewportH;
                    newY = std::max(0.0f, std::min(newY, maxScrollY));
                    if (newY != getScrollY()) { setScroll(getScrollX(), newY); markLayerDirty(); if (getScene()) UpdateScheduler::getInstance().requestPaint(); }
                    invalidate();
                }
                return true;
            } else if (key == GLFW_KEY_PAGE_DOWN) {
                if (textLayout_) {
                    auto r = textLayout_->getCursorRect(caretPos_);
                    if (preferredCursorX_ < 0.0f) preferredCursorX_ = r.x;
                    float viewportH = bounds_.height - (paddingTop_ + paddingBottom_);
                    float targetY = r.y + viewportH;
                    caretPos_ = textLayout_->getCharIndexAt(preferredCursorX_ < 0.0f ? r.x : preferredCursorX_, targetY);
                    clearSelection();
                    // adjust scroll
                    float maxScrollY = std::max(0.0f, getScrollHeight() - viewportH);
                    auto cr = textLayout_->getCursorRect(caretPos_);
                    float newY = getScrollY();
                    if (cr.y < newY) newY = cr.y;
                    if (cr.y + cr.h > newY + viewportH) newY = cr.y + cr.h - viewportH;
                    newY = std::max(0.0f, std::min(newY, maxScrollY));
                    if (newY != getScrollY()) { setScroll(getScrollX(), newY); markLayerDirty(); if (getScene()) UpdateScheduler::getInstance().requestPaint(); }
                    invalidate();
                }
                return true;
            }
        }

        return false;
    }

    bool onChar(unsigned int codepoint) override {
        if (!isFocused()) return false;
        // 如果存在选择，则替换选区
        if (hasSelection()) {
            deleteSelection();
        }
        // 将 Unicode codepoint 编码为 UTF-8 字节序列后插入（caretPos_ 为 codepoint 索引）
        std::u32string tmp;
        tmp.push_back(static_cast<char32_t>(codepoint));
        std::string utf8 = utf32_to_utf8(tmp);
        int insertByte = codepointIndexToByteIndex(caretPos_);
        value_.insert((size_t)insertByte, utf8);
        caretPos_ = static_cast<int>(caretPos_ + 1);
        // 清除 selection
        clearSelection();
        preferredCursorX_ = -1.0f;
        requestLayoutRebuild();
        updateTextLayout();
        invalidate();
        emitInputEvent();
        return true;
    }

    

    bool handleLocalScroll(ControlLocalScrollDeltaDp delta) override {
        // 单行输入无滚动内容，直接放行让父容器处理滚轮事件
        if (!multiline_) return false;

        float fontSize = fontSize_ > 0.0f ? fontSize_ : 14.0f;
        float lineHeight = lineHeight_ > 0.0f ? lineHeight_ : (fontSize * 1.2f);
        float maxScrollX = std::max(0.0f, getScrollWidth() - (bounds_.width - (paddingLeft_ + paddingRight_)));
        float maxScrollY = std::max(0.0f, getScrollHeight() - (bounds_.height - (paddingTop_ + paddingBottom_)));

        bool scrolled = false;

        // Shift+滚轮 → 水平滚动；普通滚轮 → 垂直滚动
        if (std::abs(delta.dx.value) > 0.01f || maxScrollX > 0.0f) {
            // 优先响应水平 delta（触控板水平滑动）；否则 Shift+滚轮映射到水平
            float hDelta = std::abs(delta.dx.value) > 0.01f
                ? -delta.dx.value * (lineHeight * 1.0f)
                : 0.0f;
            // 如果没有水平 delta 但有垂直，且需要水平滚动（Shift+滚轮）
            if (std::abs(hDelta) < 0.01f && std::abs(delta.dy.value) > 0.01f) {
                hDelta = -delta.dy.value * (lineHeight * 1.0f);
            }
            if (std::abs(hDelta) > 0.01f) {
                float newX = getScrollX() + hDelta;
                newX = std::max(0.0f, std::min(newX, maxScrollX));
                if (newX != getScrollX()) {
                    setScroll(newX, getScrollY());
                    markLayerDirty();
                    scrolled = true;
                }
            }
        }

        // 垂直滚动
        float scrollDelta = -delta.dy.value * (lineHeight * 3.0f);
        float newY = getScrollY() + scrollDelta;
        newY = std::max(0.0f, std::min(newY, maxScrollY));
        if (newY != getScrollY()) {
            setScroll(getScrollX(), newY);
            markLayerDirty();
            scrolled = true;
        }
        if (getScene()) {
            UpdateScheduler::getInstance().requestPaint();
            if (scrolled) emitScrollEvent();
        }
        return scrolled;
    }

private:
    bool hasSelection() const {
        return selectionStart_ >= 0 && selectionEnd_ >= 0 && selectionStart_ != selectionEnd_;
    }

    void deleteSelection() {
        if (!hasSelection()) return;
        int a = std::min(selectionStart_, selectionEnd_);
        int b = std::max(selectionStart_, selectionEnd_);
        if (a < 0) a = 0;
        // convert codepoint indices to byte offsets
        std::vector<int> idx;
        auto cps = utf8_to_utf32_with_offsets(value_, idx);
        int aByte = (a < (int)idx.size()) ? idx[a] : (int)value_.size();
        int bByte = (b < (int)idx.size()) ? idx[b] : (int)value_.size();
        preferredCursorX_ = -1.0f;
        value_.erase((size_t)aByte, (size_t)(bByte - aByte));
        caretPos_ = a;
        clearSelection();
        requestLayoutRebuild();
        updateTextLayout();
        emitInputEvent();
    }

    // Dispatch Input event to UIEventSystem when value changes
    void emitInputEvent() {
        if (!getScene()) return;
        auto scene = getScene();
        if (!scene) return;
        auto ui = scene->getUIEventSystem();
        if (!ui) return;
        UIEventSystem::UIEvent e;
        e.type = UIEventSystem::UIEventType::Input;
        e.target = this;
        e.text = value_;
        ui->dispatch(e);
    }

    void emitScrollEvent() {
        if (!getScene()) return;
        auto scene = getScene();
        if (!scene) return;
        auto ui = scene->getUIEventSystem();
        if (!ui) return;
        UIEventSystem::UIEvent e;
        e.type = UIEventSystem::UIEventType::Scroll;
        e.target = this;
        ui->dispatch(e);
    }
};
} // namespace ldt
