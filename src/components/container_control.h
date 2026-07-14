#pragma once

#include "abstract_control.h"
#include <vector>
#include <memory>
#include <algorithm>
#include "misc/float_utils.h"
#include "scrollbar_control.h"

namespace ldt {

class Scene;

/**
 * @brief 容器控件抽象类
 * 
 * 可以包含子控件的容器类控件的基类
 */
class LDT_API ContainerControl : public AbstractControl {
    friend class ControlFactory;
    friend class ControlManager;
    friend class Compositor;
public:
    // This allows access to removeChild from specific engine logic without exposing it globally.
    struct InternalAccess {
        static void removeChild(ContainerControl* container, std::shared_ptr<AbstractControl> child) {
            container->removeChild(child);
        }
    };
public:
    // Epsilon for floating point comparisons to avoid unnecessary scrollbars
    static constexpr float kScrollEpsilon = 0.01f;

    ContainerControl() = default;
    virtual ~ContainerControl() = default;

    // 当场景被设置时，传播到子控件
    void setScene(Scene* scene) override {
        AbstractControl::setScene(scene);
        for (const auto& c : children_) {
            if (c) c->setScene(scene);
        }
    }

    /**
     * @brief 添加子控件
     */
    virtual void addChild(std::shared_ptr<AbstractControl> child) {
        if (child) {
            // propagate scene if set
            if (scene_) child->setScene(scene_);
            child->setParent(this);
            children_.push_back(child);
            markLayerDirty();
            invalidateBoundingRect();
        }
    }

protected:
    /**
     * @brief 移除子控件
     */
    virtual void removeChild(std::shared_ptr<AbstractControl> child) {
        if (child) {
            child->setScene(nullptr);
            child->setParent(nullptr);
        }
        children_.erase(
            std::remove(children_.begin(), children_.end(), child),
            children_.end()
        );
        markLayerDirty();
        invalidateBoundingRect();
    }

    /**
     * @brief 清空所有子控件
     */
    virtual void clearChild() {
        children_.clear();
        markLayerDirty();
        invalidateBoundingRect();
    }

public:
    /**
     * @brief 获取所有子控件
     */
    const std::vector<std::shared_ptr<AbstractControl>>& getChild() const {
        return children_;
    }
    const std::shared_ptr<AbstractControl>& at(int idx) const {
        return children_[idx];
    }

    bool onKey(int key, int scancode, int action, int mods) override {
        // 将键事件转发给具有焦点的子控件
        for (const auto& c : children_) {
            if (c && c->isFocused()) {
                if (c->onKey(key, scancode, action, mods)) return true;
            }
            // 如果子容器，递归检查
            if (auto container = std::dynamic_pointer_cast<ContainerControl>(c)) {
                if (container->onKey(key, scancode, action, mods)) return true;
            }
        }
        return false;
    }

    bool onChar(unsigned int codepoint) override {
        for (const auto& c : children_) {
            if (c && c->isFocused()) {
                if (c->onChar(codepoint)) return true;
            }
            if (auto container = std::dynamic_pointer_cast<ContainerControl>(c)) {
                if (container->onChar(codepoint)) return true;
            }
        }
        return false;
    }

    // Handle mouse events for overlay elements (scrollbars) that sit above children.
    // Returns true if the overlay handled the event.
    bool onOverlayMouseButton(ParentLocalPointDp point, int button, int action);

    std::shared_ptr<ScrollbarControl> getVScrollbar() const { return vScrollbar_; }
    std::shared_ptr<ScrollbarControl> getHScrollbar() const { return hScrollbar_; }

    

    /**
     * @brief 通过ID查找子控件
     */
    std::shared_ptr<AbstractControl> findChildById(const std::string& id) const {
        for (const auto& child : children_) {
            if (child && child->getId() == id) {
                return child;
            }
            
            // 如果子控件也是容器，递归查找
            if (auto container = std::dynamic_pointer_cast<ContainerControl>(child)) {
                if (auto found = container->findChildById(id)) {
                    return found;
                }
            }
        }
        return nullptr;
    }

protected:
    bool handleLocalMouseMove(ControlLocalPointDp point) override;
    bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action) override;
    bool handleLocalScroll(ControlLocalScrollDeltaDp delta) override;

    /**
     * @brief 渲染所有子控件。
     *
     * @param parentClip 祖先链路累积下传的渲染裁剪区域（本容器局部坐标系）。
     *                   空矩形表示「祖先没有施加额外裁剪」，此时由本容器根据自身
     *                   是否为滚动容器（hasViewport()）决定裁剪策略。
     *
     * 设计原则：
     *   - clip 是渲染阶段的临时参数，沿递归链路逐级 intersect 传递，不保存为控件成员。
     *   - viewport 是滚动容器的固有属性（有 viewport = 可滚动），
     *     与 clip 是两个独立概念，不应混用。
     */
    void renderChildren(DisplayList& displayList, const ui::Rect& parentClip = ui::Rect()) const;

    // ── renderChildren sub-steps ──

    /**
     * @brief 解析本容器的有效裁剪区域。
     *
     * 优先级：parentClip（渲染递归传入的 clip 参数） >
     *         viewport（本容器作为滚动容器时的可见窗口，作为兜底裁剪）。
     *
     * 注意：viewport 在此仅作为"若无祖先裁剪，则以自身视口裁剪"的兜底。
     *       它不代表 viewport 与 clip 是同一概念。
     */
    ui::Rect effectiveClipRect(const ui::Rect& parentClip) const;
    bool hasScrollableContent() const;

    /** @brief 内容区域的裁剪矩形（= bounds_ 扣除 stroke），与 viewport 无关。 */
    ui::Rect contentClipRect() const;
    void pushContentClip(DisplayList& dl, const ui::Rect& clip) const;
    bool isChildVisible(const ui::Rect& clip, AbstractControl* child) const;
    void renderChildWithClip(DisplayList& dl, const ui::Rect& clip, AbstractControl* child) const;
    void renderScrollbarsIfNeeded(DisplayList& dl) const;
    void renderScrollChildrenDirect(DisplayList& dl, const ui::Rect& clip) const;
    void renderNonScrollChildrenDirect(DisplayList& dl, const ui::Rect& clip) const;
    void renderScrollChildrenCulled(DisplayList& dl, const ui::Rect& clip) const;
    void renderNonScrollChildrenCulled(DisplayList& dl, const ui::Rect& clip) const;

    /**
     * @brief 获取子树包围盒（所有子控件 bounds 的并集），用于子树级裁剪
     */
    ui::Rect getBoundingRect() const;
    void invalidateBoundingRect();

    std::vector<std::shared_ptr<AbstractControl>> children_;
    mutable std::shared_ptr<ScrollbarControl> vScrollbar_;
    mutable std::shared_ptr<ScrollbarControl> hScrollbar_;

    // 缓存的裁剪区域（仅当 isLayerDirty_ 时重新计算）
    mutable ui::Rect cachedContentClip_;

    // 子树包围盒缓存
    mutable ui::Rect cachedBoundingRect_;
    mutable bool boundingRectDirty_ = true;

    // 缓存时的滚动位置（用于滚动变化时自动失效缓存）
    mutable float cachedScrollX_ = -1.0f;
    mutable float cachedScrollY_ = -1.0f;

    // 当前帧是否处于拖拽子树中（renderChildren 入口 save/restore 模式）
    mutable bool renderDragging_ = false;
};

} // namespace ldt
