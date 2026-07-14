#include "control_manager.h"
#include "scene.h"
#include "container_control.h"
#include "abstract_control.h"
#include "overlay_event_root.h"

#include <cassert>

namespace ldt {

ControlManager::ControlManager(Scene* scene) : scene_(scene) {
    overlayRoot_ = std::make_shared<OverlayEventRoot>();
    if (scene_ && overlayRoot_) {
        overlayRoot_->setScene(scene_);
    }
}

ControlManager::~ControlManager() {
    clear();
}

void ControlManager::render(DisplayList& displayList) {
    // Draw normal controls
    for (const auto& control : controls_) {
        if (control) {
            control->render(displayList);
        }
    }

    // Draw overlay controls
    if (overlayRoot_) {
        overlayRoot_->render(displayList);
    }
}

void ControlManager::addRootControl(std::shared_ptr<AbstractControl> control) {
    if (!control) return;
    
    // 如果企图把一个带有父节点的普通子控件直接挂在顶层引发绘制，则抛出断言提示开发者
    assert(control->getParent() == nullptr && "不允许将已有父节点的控件添加为根控件！请使用 parent->addChild()");
    
    controls_.push_back(control);
    registerControlRecursive(control);
}

void ControlManager::removeRootControl(std::shared_ptr<AbstractControl> control) {
    if (control) {
        clearSceneForControl(control);
    }
    controls_.erase(
        std::remove(controls_.begin(), controls_.end(), control),
        controls_.end()
    );
}

void ControlManager::addOverlayControl(std::shared_ptr<AbstractControl> control) {
    if (!control || !overlayRoot_) return;
    overlayRoot_->addChild(control);
}

void ControlManager::removeOverlayControl(std::shared_ptr<AbstractControl> control) {
    if (!control || !overlayRoot_) return;
    overlayRoot_->removeChild(control);
}

void ControlManager::clear() {
    for (const auto& c : controls_) {
        if (c) clearSceneForControl(c);
    }
    controls_.clear();
    animatedControls_.clear();
    if (overlayRoot_) {
        for (const auto& c : overlayRoot_->getChild()) {
            if (c) clearSceneForControl(c);
        }
        overlayRoot_->clearChild();
    }
}

void ControlManager::registerControlRecursive(std::shared_ptr<AbstractControl> control) {
    if (!control || !scene_) return;
    control->setScene(scene_);
    if (auto container = std::dynamic_pointer_cast<ContainerControl>(control)) {
        for (const auto& child : container->getChild()) {
            registerControlRecursive(child);
        }
    }
}

void ControlManager::clearSceneForControl(std::shared_ptr<AbstractControl> control) {
    if (!control) return;
    control->setScene(nullptr);
    removeAnimatedControl(control.get());
    if (auto container = std::dynamic_pointer_cast<ContainerControl>(control)) {
        for (const auto& child : container->getChild()) {
            clearSceneForControl(child);
        }
    }
}

void ControlManager::addAnimatedControl(AbstractControl* ctrl) {
    if (!ctrl) return;
    for (auto c : animatedControls_) {
        if (c == ctrl) return;
    }
    animatedControls_.push_back(ctrl);
}

void ControlManager::removeAnimatedControl(AbstractControl* ctrl) {
    if (!ctrl) return;
    animatedControls_.erase(
        std::remove(animatedControls_.begin(), animatedControls_.end(), ctrl),
        animatedControls_.end()
    );
}

bool ControlManager::processAnimatedControls() {
    if (animatedControls_.empty()) return false;
    auto list = animatedControls_;
    animatedControls_.clear();
    for (auto* c : list) {
        if (c) c->update();
    }
    return true;
}

std::shared_ptr<AbstractControl> ControlManager::findControlById(const std::string& id) const {
    for (const auto& control : controls_) {
        if (control && control->getId() == id) {
            return control;
        }
        if (auto container = std::dynamic_pointer_cast<ContainerControl>(control)) {
            if (auto found = container->findChildById(id)) {
                return found;
            }
        }
    }
    return nullptr;
}

} // namespace ldt
