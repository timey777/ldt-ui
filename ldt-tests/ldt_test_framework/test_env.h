#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <iostream>

#include "engine/document_runtime.h"
#include "engine/style_engine.h"
#include "engine/layout_engine.h"
#include "engine/astbuild_engine.h"
#include "engine/box_model_engine.h"
#include "engine/core/parse.h"
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_node.h"
#include "engine/core/tree_synchronizer.h"
#include "render/text_measurer.h"
#include "render/display_list.h"
#include "render/drawer.h"
#include "components/scene.h"
#include "components/control_manager.h"
#include "components/container_control.h"
#include "components/control_factory.h"
#include "components/input.h"
#include "misc/perf_timer.h"

// ============================================================
// StubTextLayout — 模拟文本布局，支持 wrap 判断
// ============================================================
class StubTextLayout : public ITextLayout {
public:
    std::string text_;
    float lineHeight_ = 20.0f;
    float charWidth_ = 8.0f;
    float maxWidth_ = -1.0f;
    bool wrap_ = false;
    int lineCount_ = 1;

    StubTextLayout(const std::string& text, float lineH, float charW, float maxW, bool wrap)
        : text_(text), lineHeight_(lineH), charWidth_(charW), maxWidth_(maxW), wrap_(wrap) {
        rebuild();
    }

    void rebuild() {
        if (text_.empty()) { lineCount_ = 1; return; }
        if (wrap_ && maxWidth_ > 0) {
            int charsPerLine = std::max(1, static_cast<int>(maxWidth_ / charWidth_));
            // count UTF-8 chars
            size_t charCount = 0;
            for (size_t i = 0; i < text_.size(); i++)
                if ((text_[i] & 0xC0) != 0x80) charCount++;
            lineCount_ = std::max(1, (static_cast<int>(charCount) + charsPerLine - 1) / charsPerLine);
        } else {
            lineCount_ = 1;
        }
    }

    render::Rect getBounds() const override {
        float w = wrap_ && maxWidth_ > 0 ? maxWidth_ : (text_.size() * charWidth_);
        return {0, 0, w, lineHeight_ * lineCount_};
    }

    int getCharIndexAt(float x, float y) const override { (void)x; (void)y; return 0; }
    render::Rect getCursorRect(int index) const override { (void)index; return {0,0,1,lineHeight_}; }
    std::vector<render::Rect> getSelectionRects(int start, int end) const override {
        (void)start; (void)end; return {};
    }
    std::string getText() const override { return text_; }
    int getLineCount() const override { return lineCount_; }
};

// ============================================================
// StubDrawer — 满足 Input::updateTextLayout 的 IDrawer
// 继承 StubTextMeasurer 以同时满足 ITextMeasurer
// ============================================================
class StubDrawer : public ITextMeasurer, public IDrawer {
public:
    float charWidth = 8.0f;
    float lineHeightVal = 20.0f;
    float ascentVal = 16.0f;
    float descentVal = 4.0f;

    // ---- ITextMeasurer ----
    TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) override {
        (void)font;
        TextMetrics m;
        if (text.empty()) {
            m.width = 0; m.ascent = ascentVal; m.descent = descentVal; m.lineHeight = lineHeightVal;
            return m;
        }
        float avgCharW = charWidth * (font.size / 14.0f);
        float lineH = lineHeightVal * (font.size / 14.0f);
        float textW = static_cast<float>(text.size()) * avgCharW;
        if (wrapWidth > 0 && textW > wrapWidth) {
            int cpl = std::max(1, static_cast<int>(wrapWidth / avgCharW));
            int lines = std::max(1, (static_cast<int>(text.size()) + cpl - 1) / cpl);
            m.width = wrapWidth; m.lineHeight = lineH * lines; m.ascent = ascentVal; m.descent = descentVal;
        } else {
            m.width = textW; m.lineHeight = lineH; m.ascent = ascentVal; m.descent = descentVal;
        }
        return m;
    }

    TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) override {
        size_t charCount = 0;
        for (size_t i = 0; i < utf8text.size(); i++)
            if ((utf8text[i] & 0xC0) != 0x80) charCount++;
        std::u32string u32; u32.resize(charCount, U'x');
        return measureText(u32, font, wrapWidth);
    }

    float getLineHeight(const FontDesc& font) override {
        return lineHeightVal * (font.size / 14.0f);
    }

    // ---- IDrawer (大部分空实现) ----
    void drawRect(float x, float y, float w, float h, const DrawStyle& style) override { (void)x;(void)y;(void)w;(void)h;(void)style; }
    void drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style) override { (void)x;(void)y;(void)w;(void)h;(void)rx;(void)ry;(void)style; }
    void drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style) override { (void)img;(void)x;(void)y;(void)w;(void)h;(void)style; }
    void drawText(const std::string& text, float x, float y, const Font& font, const DrawStyle& style, float maxWidth, const TextLayoutParams& layout) override { (void)text;(void)x;(void)y;(void)font;(void)style;(void)maxWidth;(void)layout; }

    std::shared_ptr<ITextLayout> createTextLayout(const std::string& text, const Font& font, const TextLayoutParams& params, float maxWidth = -1.0f) override {
        (void)font;
        return std::make_shared<StubTextLayout>(text, lineHeightVal * (font.size / 14.0f),
                                                charWidth * (font.size / 14.0f),
                                                maxWidth, params.wrap);
    }

    void drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style) override { (void)layout;(void)x;(void)y;(void)style; }
    ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) override { (void)data;(void)width;(void)height;(void)channels; return 0; }
    void unloadImage(ImageHandle img) override { (void)img; }
    void setClipRect(const render::Rect& r) override { (void)r; }
    bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) override { (void)r; if(outNewClip)*outNewClip=r; return true; }
    bool getClipRect(render::Rect* outCurrent) const override { if(outCurrent)*outCurrent=render::Rect{0,0,800,600}; return true; }
    void resetClip() override {}
    void setClipCornerRadius(float radius) override { (void)radius; }
    float getClipCornerRadius() const override { return 0.0f; }
    void pushTransform(float dx, float dy) override { (void)dx; (void)dy; }
    void popTransform() override {}
};

// ============================================================
// TestEnv — 管理 DocumentRuntime 的最小测试环境
// ============================================================
class TestEnv {
public:
    DocumentRuntime runtime;
    StyleEngine styleEngine;
    LayoutEngine layoutEngine;
    ASTBuildEngine astBuildEngine;
    BoxModelEngine boxModelEngine;
    StubDrawer stubDrawer;         // 同时实现 ITextMeasurer + IDrawer
    ldt::TreeSynchronizer treeSync;
    std::unique_ptr<ldt::Scene> scene;  // 用于 control sync 的场景

    TestEnv() {
        ldt::PerfLog::setEnabled(false);

        runtime.registerService(&styleEngine);
        runtime.registerService(&layoutEngine);
        runtime.registerService(&astBuildEngine);
        runtime.registerService(&boxModelEngine);

        runtime.setTextMeasurer(&stubDrawer);

        runtime.initializeAll({
            typeid(StyleEngine),
            typeid(LayoutEngine),
            typeid(ASTBuildEngine),
            typeid(BoxModelEngine)
        });
    }

    ~TestEnv() {
        runtime.shutdownAll();
    }

    // 解析 LDT DSL 并运行完整管线
    // 返回 ResolvedTree 根节点（若解析失败返回 nullptr）
    ldt::ResolvedNode* load(const std::string& ldtSource, float viewportW = 800, float viewportH = 600) {
        // 注册 parser（幂等：重复注册不影响）
        ldt::ParserRegistry::instance().registerParser("style", std::make_shared<ldt::StyleParser>());
        ldt::ParserRegistry::instance().registerParser("layout", std::make_shared<ldt::LayoutParser>());
        ldt::ParserRegistry::instance().registerParser("content", std::make_shared<ldt::ContentParser>());

        ldt::LDTParser parser;
        auto snippet = parser.parse(ldtSource);
        std::shared_ptr<ASTNode> root = snippet.root();
        if (!root) return nullptr;

        runtime.notifyASTUpdated(root);

        // 运行管线（到 DisplayList 为止，但不需要真正的渲染器）
        ldt::DisplayList scratch;
        runtime.runPipeline(scratch, viewportW, viewportH);

        auto* tree = runtime.getResolvedTree();
        if (!tree) return nullptr;
        return tree->getRoot();
    }

    // 加载 LDT + 运行管线 + 创建控件（完整流程）
    // 返回 Scene 指针，可以通过 Scene 访问控件
    ldt::Scene* loadFull(const std::string& ldtSource, float viewportW = 800, float viewportH = 600) {
        // 先运行基础管线
        auto* root = load(ldtSource, viewportW, viewportH);
        if (!root) return nullptr;

        // 创建 Scene
        if (!scene) {
            scene = std::make_unique<ldt::Scene>(runtime);
        }

        // 将引擎的 ResolvedTree 合并到 Scene 的 ResolvedTreeView
        auto* sceneTree = scene->getResolvedTree();
        auto* engineTree = runtime.getResolvedTree();
        if (sceneTree && engineTree && engineTree->getRoot()) {
            if (!sceneTree->getRoot()) {
                sceneTree->attachSubtreeFromOther(*engineTree, nullptr, false);
            }
        }

        // 运行树同步 → 创建控件，调用各控件的 setup/updateTextLayout
        ldt::SyncScope scope;
        scope.resolvedRoot = scene->getResolvedTree()->getRoot();
        if (scope.resolvedRoot) {
            treeSync.syncToControls(&runtime, scene.get(), scope);
        }

        return scene.get();
    }

public:
    // 按 id 查找 ResolvedNode
    ldt::ResolvedNode* findById(const std::string& id) {
        auto* tree = runtime.getResolvedTree();
        if (!tree || !tree->getRoot()) return nullptr;
        return findByIdRecursive(tree->getRoot(), id);
    }

private:
    static ldt::ResolvedNode* findByIdRecursive(ldt::ResolvedNode* node, const std::string& id) {
        if (!node) return nullptr;
        if (node->getId() == id) return node;
        for (auto* child : node->getChildren()) {
            auto* found = findByIdRecursive(child, id);
            if (found) return found;
        }
        return nullptr;
    }
};
