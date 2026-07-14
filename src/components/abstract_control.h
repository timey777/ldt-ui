#pragma once

#include "render/display_list.h"
#include "engine/core/coordinate_types.h"
#include <string>
#include <memory>

// forward-declare ASTNode (defined in parse.h, kept out of this header to avoid heavy coupling)
struct ASTNode;

namespace ldt {

class Scene;

/**
 * @brief 控件抽象基类
 * 
 * 所有UI控件的基类，定义了控件的通用属性和行为
 */
class LDT_API AbstractControl {
public:
    AbstractControl();
    virtual ~AbstractControl() = default;

    // 禁止拷贝
    AbstractControl(const AbstractControl&) = delete;
    AbstractControl& operator=(const AbstractControl&) = delete;

    /**
     * @brief 渲染控件到显示列表 (模板方法，处理通用逻辑如视觉平移)
     * @param displayList 显示列表
     * @param clip 祖先链路累积下传的渲染裁剪区域（本控件局部坐标系），
     *             空矩形表示无祖先裁剪约束。
     *             clip 是渲染阶段的临时参数，不属于控件，仅沿递归传递。
     */
    void render(DisplayList& displayList, const ui::Rect& clip = ui::Rect()) const;

protected:
    /**
     * @brief 实际的控件绘制逻辑，由子类实现
     * @param clip 当前有效的渲染裁剪区域，由 render() 透传。
     *             大多数叶子控件不需要感知它（依赖 DisplayList::pushClip 裁剪即可），
     *             容器控件用它做子树级可见性剔除。
     */
    virtual void onRender(DisplayList& displayList, const ui::Rect& clip) const = 0;

public:
    /**
     * @brief 每帧调用用于更新控件的动画/逻辑（无渲染），由 Scene 驱动
     */
    virtual void update() {}

    /**
     * @brief 将控件加入场景的动画更新队列，下一帧会调用 `update()`。
     */
    virtual void invalidate();

    /**
     * @brief 设置控件所属场景（由 Scene::addControl 调用）
     */
    virtual void setScene(Scene* scene);
    Scene* getScene() const;

    /**
     * @brief 设置父控件
     */
    virtual void setParent(AbstractControl* parent);
    AbstractControl* getParent() const;

    /**
     * @brief 获取控件类型名称
     */
    virtual std::string getTypeName() const = 0;

    // 位置和大小
    virtual void setBounds(const ui::Rect& bounds);
    
    const ui::Rect& getBounds() const;

    // 颜色
    virtual void setFillColor(const ui::Color& color);
    const ui::Color& getFillColor() const;

    void setStrokeColor(const ui::Color& color);
    const ui::Color& getStrokeColor() const;

    // 边框
    void setStrokeWidth(const ui::Edges& width);
    const ui::Edges& getStrokeWidth() const;

    void setCornerRadius(float radius);
    float getCornerRadius() const;

    void setBackgroundImage(const std::string& path);
    const std::string& getBackgroundImage() const;

    // 可见性
    void setVisible(bool visible);
    bool isVisible() const;

    // 启用/禁用
    virtual void setEnabled(bool enabled);
    bool isEnabled() const;

    // ID
    void setId(const std::string& id);
    const std::string& getId() const;

    // internal diffing/mapping between AST nodes and controls.
    void setUid(const std::string& uid);
    const std::string& getUid() const;

    // class list (set from AST during control creation)
    void setClassList(const std::vector<std::string>& cls, bool forceLayout = false);
    const std::vector<std::string>& getClassList() const;
    bool hasClass(const std::string& c) const;
    void addClass(const std::string& c, bool forceLayout = false);
    void removeClass(const std::string& c, bool forceLayout = false);


    // ----------------- 事件接口 -----------------
    bool onLocalMouseMove(ControlLocalPointDp point);
    bool onLocalMouseButton(ControlLocalPointDp point, int button, int action);
    ControlLocalPointDp toLocalPoint(LogicalPointDp point) const;
    bool hitTestLocal(ControlLocalPointDp point) const;
    bool hitTestPoint(LogicalPointDp point) const;

    // 键盘事件（返回 true 表示已处理）
    virtual bool onKey(int key, int scancode, int action, int mods);
    // 文本输入（UTF-32 codepoint from GLFW char callback）
    virtual bool onChar(unsigned int codepoint);

    bool onLocalScroll(ControlLocalScrollDeltaDp delta);

    // 焦点管理
    // 注意：焦点的高层管理由 `Scene` 负责（负责决定哪个控件获得/失去焦点并调用
    // 控件的 `onFocus` / `onBlur`）。控件应在接收到 `onFocus` 时更新自身状态。
    virtual void onFocus();
    virtual void onBlur();
    bool isFocused() const;

    // 悬停状态回调（由 UIEventSystem 驱动）
    virtual void onHover(bool isHovered);

    // 激活状态回调（由 UIEventSystem 驱动，对应鼠标按下）
    virtual void onActive(bool isActive);

    virtual void onDragStart();
    void onLogicalDragMove(LogicalDragOffsetDp delta);
    virtual void onDrop();
    // Mouse capture helpers: controls can request the Scene to capture/release mouse.
    // These are convenience methods; the actual capture state is stored in Scene.
    virtual bool wantsMouseCapture() const;
    // 如果 relativeTo 不为空，则捕获期间鼠标坐标将相对于 relativeTo 的局部坐标系计算
    void captureMouse(AbstractControl* relativeTo = nullptr);
    void releaseMouse();

    // 如果控件承担可滚动或可见区域的职责，覆盖该方法以确保控件在视口中可见。
    // 默认实现为空（不做滚动）。
    virtual void ensureVisible() {}

    // 滚动相关
    void setScroll(float x, float y);
    float getScrollX() const;
    float getScrollY() const;
    
    virtual void setScrollSize(float w, float h);
    float getScrollWidth() const;
    float getScrollHeight() const;

    /**
     * @brief 视口大小 — 滚动容器的可见窗口尺寸。
     *
     * 概念定义：
     *   - viewport 是滚动容器的固有属性。它表示"透过哪个窗口看到可滚动内容"。
     *   - 只有 overflow != visible 的容器才拥有 viewport
     *     （即 hasViewport() == true）。
     *   - viewport 属于控件本身，跨帧持久，由布局阶段设置。
     *
     * viewport 与 clip 是两个独立概念：
     *   - viewport = 滚动视口（谁拥有？→ 滚动容器）
     *   - clip     = 渲染裁剪（谁拥有？→ 没有谁。它是 render 的临时参数，
     *                沿递归链路逐级 intersect 传递）
     *
     * 不要把 viewport 当成"是否裁剪子控件"的开关。
     * 裁剪决策应由渲染阶段的 clip 参数决定。
     *
     * @see docs/VIEWPORT_VS_CLIP_DESIGN.md
     */
    virtual void setViewportSize(float w, float h);
    float getViewportWidth() const;
    float getViewportHeight() const;
    bool hasViewport() const;

    // 视觉位移 (用于拖拽等高频平移优化，不触发重排)
    void setVisualOffset(float x, float y);
    float getVisualOffsetX() const { return visualOffsetX_; }
    float getVisualOffsetY() const { return visualOffsetY_; }

    // NOTE: 控件不再包含任何 AST 或回写相关的接口。所有运行时到 AST 的同步
    // 都应由宿主层（例如 Scene 或解析层）负责执行。此处不再声明持久化回调。

    // Padding (视觉内边距，用于内容绘制时的偏移)
    virtual void setPadding(float left, float top, float right, float bottom);
    float getPaddingLeft() const { return paddingLeft_; }
    float getPaddingTop() const { return paddingTop_; }
    float getPaddingRight() const { return paddingRight_; }
    float getPaddingBottom() const { return paddingBottom_; }

    // 标记图层脏（用于局部重绘优化）
    virtual void markLayerDirty();

protected:
    virtual bool handleLocalHitTest(ControlLocalPointDp point) const;
    virtual bool handleLogicalHitTest(LogicalPointDp point) const;
    virtual bool handleLocalMouseMove(ControlLocalPointDp point);
    virtual bool handleLocalMouseButton(ControlLocalPointDp point, int button, int action);
    virtual bool handleLocalScroll(ControlLocalScrollDeltaDp delta);
    virtual void handleLogicalDragMove(LogicalDragOffsetDp delta);

    std::string id_;
    std::vector<std::string> classes_;
    std::string uid_;
    ui::Rect bounds_;
    ui::Color fillColor_{0.0f, 0.0f, 0.0f, 0.0f};  // 默认透明
    ui::Color strokeColor_{0.0f, 0.0f, 0.0f, 1.0f};
    ui::Edges strokeWidth_;
    float cornerRadius_ = 0.0f;
    std::string backgroundImage_;
    bool visible_ = true;
    bool enabled_ = true;
    bool focused_ = false;
    
    float scrollWidth_ = 0.0f;
    float scrollHeight_ = 0.0f;
    float viewportWidth_ = 0.0f;
    float viewportHeight_ = 0.0f;
    //拖动偏移
    float dragStartPosX_ = 0.0f;
    float dragStartPosY_ = 0.0f;
    // Padding
    float paddingLeft_ = 0.0f;
    float paddingTop_ = 0.0f;
    float paddingRight_ = 0.0f;
    float paddingBottom_ = 0.0f;

    // 所属 Scene（若有）
    Scene* scene_ = nullptr;
    AbstractControl* parent_ = nullptr;
    
    // 局部重绘优化
    mutable std::shared_ptr<DisplayList> layerDisplayList_;
    mutable bool isLayerDirty_ = true;

    // NOTE: 控件不应直接链接或引用 AST 节点（避免控件持有解析数据），
    // 运行时到 AST 的同步由外部层（Scene/host）负责。
private:
    // 视觉位移 (不参与布局)
    float visualOffsetX_ = 0.0f;
    float visualOffsetY_ = 0.0f;

    // 滚动状态
    float scrollX_ = 0.0f;
    float scrollY_ = 0.0f;
    
};

// invalidate() implementation moved to cpp to avoid requiring full Scene definition here

} // namespace ldt
