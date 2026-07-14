#pragma once

#include "render/display_list.h"
#include "render/renderer.h"
#include "core/coordinate_types.h"
#include "core/parse.h"
#include <memory>
#include <string>
#include <typeindex>
#include "ldt_export.h"

namespace ldt {

// 纯合成器/呈现器：仅管理 DisplayList 的提交、叠加与绘制。
// 不再负责管道执行、树同步等更新协调逻辑（已迁移至 ViewCoordinator）。
class LDT_API Compositor {
public:
    Compositor();
    virtual ~Compositor();

    void setRenderer(Renderer* renderer);
    Renderer* getRenderer() const;

    void submitDisplayList(const DisplayList& displayList);
    void submitDisplayList(DisplayList&& displayList);
    void submitOverlayDisplayList(const DisplayList& overlay);
    void submitOverlayDisplayList(DisplayList&& overlay);

    void paintAll();
    void paintAllTo(const ui::Rect& target, bool presentAfter = false);

    void clear();
    void markDirty(float x, float y, float w, float h);

    // 从 Scene 渲染一帧并提交为当前 DisplayList（清除旧内容）。
    void renderScene(class Scene* scene);

    bool saveDisplayListsToFile(const std::string& path) const;
    bool loadDisplayListsFromFile(const std::string& path);

private:
    Renderer* renderer_ = nullptr;
    std::vector<DisplayList> displayLists_;
    DisplayList overlayDisplayList_;
    bool isDirty_ = false;
};

} // namespace ldt
