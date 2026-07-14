#pragma once

#include "ldt_export.h"
#include "render/display_list.h"
#include "abstract_control.h"
#include "overlay_event_root.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace ldt {

class Scene;
class SceneRender; 

/**
 * @brief 控件管理器 - 管理场景中的所有控件及其生命周期和渲染顺序
 *        ControlManager 接管了原先 Scene 管理控件的职责。
 */
class LDT_API ControlManager {
public:
    ControlManager(Scene* scene);
    ~ControlManager();

    /**
     * @brief 渲染场景中的所有内容到 DisplayList（包含主控件树与覆盖层）
     */
    void render(DisplayList& displayList);

    /**
     * @brief 添加一个根控件
     */
    void addRootControl(std::shared_ptr<AbstractControl> control);

    /**
     * @brief 移除一个根控件
     */
    void removeRootControl(std::shared_ptr<AbstractControl> control);

    /**
     * @brief 添加覆盖层控件
     */
    void addOverlayControl(std::shared_ptr<AbstractControl> control);

    /**
     * @brief 移除覆盖层控件
     */
    void removeOverlayControl(std::shared_ptr<AbstractControl> control);

    /**
     * @brief 清空所有控件
     */
    void clear();

    /**
     * @brief 查找控件
     */
    std::shared_ptr<AbstractControl> findControlById(const std::string& id) const;

    /**
     * @brief 获取所有普通控件
     */
    const std::vector<std::shared_ptr<AbstractControl>>& getControls() const {
        return controls_;
    }

    /**
     * @brief 获取覆盖层根节点
     */
    std::shared_ptr<OverlayEventRoot> getOverlayRoot() const {
        return overlayRoot_;
    }

    // 动画更新队列相关
    void addAnimatedControl(AbstractControl* ctrl);
    void removeAnimatedControl(AbstractControl* ctrl);
    bool processAnimatedControls();

    // 辅助方法：设置 Scene 上下文（通常只在初始化时调用）
    void setScene(Scene* scene) { scene_ = scene; }

private:
    Scene* scene_;
    std::vector<std::shared_ptr<AbstractControl>> controls_;
    std::shared_ptr<OverlayEventRoot> overlayRoot_;
    std::vector<AbstractControl*> animatedControls_;

    void clearSceneForControl(std::shared_ptr<AbstractControl> control);
    void registerControlRecursive(std::shared_ptr<AbstractControl> control);
};

} // namespace ldt
