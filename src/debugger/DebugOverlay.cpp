#include "DebugOverlay.h"
#include "components/panel.h"
#include "components/text.h"
#include "components/scene.h"
#include "components/control_factory.h"
#include <cstdio>

namespace ldt {

void DebugOverlay::buildControls(Scene* scene) {
    if (built_) return;

    auto* factory = ControlFactory::getInstance();
    panel_ = factory->CreatePanel();
    panel_->setScene(scene);
    panel_->setFillColor(ui::Color(0.0f, 0.0f, 0.0f, 0.72f));
    panel_->setStrokeColor(ui::Color(0.2f, 0.2f, 0.2f, 0.8f));
    panel_->setStrokeWidth(ui::Edges(1.0f));
    panel_->setCornerRadius(4.0f);

    constexpr int kNumRows = 16; // 2+5+5+4
    rows_.reserve(kNumRows);
    for (int i = 0; i < kNumRows; ++i) {
        auto text = factory->CreateText();
        text->setScene(scene);
        text->setWrap(false);
        text->setFontSize(kFontSize);
        rows_.push_back(text);
        panel_->addChild(text);
    }

    const int sectionIdx[] = {2, 7, 12};
    for (int si : sectionIdx) {
        rows_[(size_t)si]->setFontSize(kFontSize + 1.0f);
        rows_[(size_t)si]->setTextColor(ui::Color(1.0f, 1.0f, 1.0f, 0.9f));
    }

    built_ = true;
}

float DebugOverlay::computePanelWidth(Scene* scene) {
    auto* measurer = scene ? scene->getTextMeasurer() : nullptr;
    auto est = [measurer](const std::string& s, float fs) -> float {
        if (measurer) { FontDesc fd{"", fs}; return measurer->measureText(s, fd, -1.0f).width; }
        return (float)s.size() * fs * 0.6f;
    };
    // Measure worst-case combined "label: value" strings
    float maxW = est("  TxtLayouts: 50000", kFontSize);
    float w2 = est("  Detail:   50000 t / 50000 tl / 5 r", kFontSize);
    if (w2 > maxW) maxW = w2;
    float w3 = est("  Clip:     50000/50000", kFontSize);
    if (w3 > maxW) maxW = w3;
    return maxW + kPadX * 2 + 4.0f; // +4 for safety margin
}

void DebugOverlay::updateContent(const RenderStats& stats, double avgFrameUs) {
    if (!panel_ || rows_.empty()) return;
    double fps = (avgFrameUs > 0.0) ? (1000000.0 / avgFrameUs) : 0.0;
    double cpuMs = avgFrameUs / 1000.0;
    char buf[128];
    float panelW = panel_->getBounds().width;
    float lx = kOriginX + kPadX;
    float lw = panelW - kPadX * 2;
    float cy = kOriginY + kPadY;
    int ri = 0;

    auto setRow = [&](const char* text, const ui::Color& c) {
        rows_[ri]->setText(text);
        rows_[ri]->setTextColor(c);
        rows_[ri]->setBounds(ui::Rect(lx, cy, lw, kLineHeight));
        ri++; cy += kLineHeight;
    };

    // ── FPS / CPU ──
    snprintf(buf, sizeof(buf), "FPS:  %.0f", fps);
    setRow(buf, ui::Color(0.0f, 1.0f, 0.3f, 1.0f));

    snprintf(buf, sizeof(buf), "CPU: %.2f ms", cpuMs);
    setRow(buf, ui::Color(0.0f, 1.0f, 0.3f, 1.0f));

    cy += kSectionGap;

    // ── Nodes ──
    setRow("Nodes", ui::Color(1.0f, 1.0f, 1.0f, 0.9f));

    snprintf(buf, sizeof(buf), "  Total:   %d", stats.totalControls);
    setRow(buf, ui::Color(0.8f, 0.8f, 0.8f, 1.0f));

    snprintf(buf, sizeof(buf), "  Visited: %d", stats.visitedNodes);
    setRow(buf, stats.visitedNodes > stats.totalControls / 2
               ? ui::Color(1.0f, 0.5f, 0.2f, 1.0f)
               : ui::Color(0.3f, 1.0f, 0.3f, 1.0f));

    snprintf(buf, sizeof(buf), "  Visible: %d", stats.visibleNodes);
    setRow(buf, ui::Color(0.3f, 1.0f, 0.3f, 1.0f));

    snprintf(buf, sizeof(buf), "  Culled:  %d", stats.culledNodes);
    setRow(buf, ui::Color(0.5f, 0.5f, 0.5f, 1.0f));

    cy += kSectionGap;

    // ── Renderer ──
    setRow("Renderer", ui::Color(1.0f, 1.0f, 1.0f, 0.9f));

    snprintf(buf, sizeof(buf), "  Draw Cmd: %d", stats.totalCommands);
    setRow(buf, ui::Color(0.3f, 1.0f, 0.3f, 1.0f));

    snprintf(buf, sizeof(buf), "  Clip:     %d/%d",
             stats.pushClipCommands, stats.popClipCommands);
    setRow(buf, ui::Color(0.6f, 0.6f, 0.6f, 1.0f));

    snprintf(buf, sizeof(buf), "  SubLists: %d", stats.subLists);
    setRow(buf, ui::Color(0.6f, 0.6f, 0.6f, 1.0f));

    snprintf(buf, sizeof(buf), "  Detail:   %d t / %d tl / %d r",
             stats.textCommands, stats.textLayoutCommands, stats.rectCommands);
    setRow(buf, ui::Color(0.5f, 0.5f, 0.5f, 1.0f));

    cy += kSectionGap;

    // ── Memory ──
    setRow("Memory", ui::Color(1.0f, 1.0f, 1.0f, 0.9f));

    snprintf(buf, sizeof(buf), "  TxtLayouts: %d", stats.textLayoutCommands);
    setRow(buf, ui::Color(0.6f, 0.6f, 0.6f, 1.0f));

    setRow("  Glyph Atlas: --", ui::Color(0.4f, 0.4f, 0.4f, 1.0f));
    setRow("  Vtx Buffer:  --", ui::Color(0.4f, 0.4f, 0.4f, 1.0f));
}

void DebugOverlay::render(DisplayList& dl, const RenderStats& stats,
                          double avgFrameUs, Scene* scene) {
    if (!visible_) return;
    if (lastTotalControls_ != stats.totalControls) {
        built_ = false;
        lastTotalControls_ = stats.totalControls;
    }
    buildControls(scene);
    if (!panel_) return;

    float panelW = computePanelWidth(scene);
    constexpr int kTotalRows = 16;
    constexpr int kSections = 3;
    float panelH = kPadY * 2 + kTotalRows * kLineHeight + kSections * kSectionGap;

    panel_->setBounds(ui::Rect(kOriginX, kOriginY, panelW, panelH));
    panel_->setViewportSize(panelW, panelH);
    updateContent(stats, avgFrameUs);
    panel_->render(dl);
}

bool DebugOverlay::onKey(int key, int action) {
    if (key == 292 /* GLFW_KEY_F3 */ && action == 1 /* press */) {
        toggle();
        return true;
    }
    return false;
}

} // namespace ldt
