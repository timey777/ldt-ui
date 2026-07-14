#pragma once

#include "graphics_types.h"
#include "display_list.h"
#include <memory>
#include <map>
#include "ldt_export.h"

class IDrawer;

namespace ldt {

// 高层渲染器：负责将 DisplayList 转换为 IDrawer 的底层绘制调用
class LDT_API Renderer {
public:
    explicit Renderer(IDrawer* drawer);
    ~Renderer();

    // 设置底层绘制器
    void setDrawer(IDrawer* drawer);
    IDrawer* getDrawer() const;

    // 渲染 DisplayList
    void render(const DisplayList& displayList);
    void render2(const DisplayList& displayList);
    // 渲染到指定目标矩形（目标以当前画布坐标系为准，支持偏移与裁剪）
    void render(const DisplayList& displayList, const ui::Rect& target);

    // 资源管理
    void clearImageCache();
    
    // 更新加载队列（在主线程调用，用于上传纹理）
    void updateImageLoadQueue();
private:
    struct ClipState {
        render::Rect rect{};
        float cornerRadius = 0.0f;
        bool hasClip = false;
    };

    // 转换颜色格式
    render::Color convertColor(const ui::Color& color);

    // 渲染单个命令
    void renderCommand(const DrawCommand& cmd);

    // 渲染矩形
    void renderRect(const DrawCommand& cmd);

    // 渲染文本
    void renderText(const DrawCommand& cmd);
    void renderTextLayout(const DrawCommand& cmd);

    // 渲染图片
    void renderImage(const DrawCommand& cmd);

    void renderPushClip(const DrawCommand& cmd);
    void renderPopClip();

    void renderPushTransform(const DrawCommand& cmd);
    void renderPopTransform();

    // 获取或加载图片
    ImageHandle getOrLoadImage(const std::string& path);

    IDrawer* drawer_;
    std::map<std::string, ImageHandle> imageCache_;
    std::vector<ClipState> clipStateStack_;
};

} // namespace ldt
