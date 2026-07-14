#pragma once
#include <string>
#include <memory>
#include "components/scene.h"
#include "ldt_export.h"

namespace ldt {

class LDT_API UIComponent {
public:
    virtual ~UIComponent() = default;

    /**
     * @brief 将组件挂载到指定的 Scene 节点下
     * @param scene 目标场景
     * @param parentId 父节点 ID
     */
    virtual void onAttach(Scene* scene, const std::string& parentId) = 0;

    /**
     * @brief 组件卸载时的清理逻辑
     */
    virtual void onDetach(Scene* scene) {}
};

} // namespace ldt
