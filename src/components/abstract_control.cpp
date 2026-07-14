#include "abstract_control.h"
#include "scene.h"
#include "control_factory.h"
#include "misc/float_utils.h"
#include "components/scroll_helper.h"
#include "control_manager.h"
#include <algorithm>
#include <engine/update_scheduler.h>
#include <engine/box_model_engine.h>
#include "engine/document_runtime.h"

namespace ldt {
	AbstractControl::AbstractControl() {
	}

	void AbstractControl::render(DisplayList& displayList, const ui::Rect& clip) const {
		if (!visible_) return;

		bool hasTransform = (visualOffsetX_ != 0.0f || visualOffsetY_ != 0.0f);
		if (hasTransform) {
			displayList.pushTransform(visualOffsetX_, visualOffsetY_);
		}

		// 调用子类的具体绘制逻辑，clip 沿递归链路透传
		onRender(displayList, clip);

		if (hasTransform) {
			displayList.popTransform();
		}
	}

	void AbstractControl::setBounds(const ui::Rect& bounds) {
		// Only mark dirty if bounds actually changed (use float tolerance)
		if (ldt::floatNotEqual(bounds_.x, bounds.x) ||
			ldt::floatNotEqual(bounds_.y, bounds.y) ||
			ldt::floatNotEqual(bounds_.width, bounds.width) ||
			ldt::floatNotEqual(bounds_.height, bounds.height)) {
			bounds_ = bounds;
			markLayerDirty();
		}
	}

	void AbstractControl::invalidate() {
		markLayerDirty();
		if (scene_ && scene_->getControlManager()) {
			scene_->getControlManager()->addAnimatedControl(this);
		}
	}

	void AbstractControl::captureMouse(AbstractControl* relativeTo) {
		if (scene_) {
			ResolvedNode* rn = nullptr;
			std::string uid = getUid();
			if (!uid.empty()) {
				rn = ControlFactory::getInstance()->FindNodeByUid(uid);
			}
			ResolvedNode* relRn = nullptr;
			if (relativeTo){
				std::string relUid = relativeTo->getUid();
				if(!relUid.empty()) relRn = ControlFactory::getInstance()->FindNodeByUid(relUid);
			}
			if(rn) scene_->setMouseCapture(rn, relRn);
			else scene_->setMouseCaptureControl(this, relativeTo);
		}
	}

	void AbstractControl::releaseMouse() {
		if (scene_) scene_->clearMouseCapture();
	}

	void AbstractControl::setScroll(float x, float y)
	{
		scrollX_ = x;
		scrollY_ = y;
		std::string uid = getUid();
		if (!uid.empty()) {
			ResolvedNode* rn = ControlFactory::getInstance()->FindNodeByUid(uid);
			if (rn) {
				if (ldt::floatNotEqual(rn->layout.scroll.offsetX, x) || ldt::floatNotEqual(rn->layout.scroll.offsetY, y)) {
					rn->layout.scroll.offsetX = x;
					rn->layout.scroll.offsetY = y;
					rn->markDirty(DirtyFlag::Paint);
				}
			}
		}
	}

	void AbstractControl::setScrollSize(float w, float h) {
		scrollWidth_ = w;
		scrollHeight_ = h;
		// Clamp scroll to current bounds; if content fits, this will reset scroll to 0
		if (ldt::ScrollHelper::clampAndApply(this, getViewportWidth(), getViewportHeight(),
			scrollWidth_, scrollHeight_, getScrollX(), getScrollY())) {
			// UpdateScheduler::getInstance().requestPaint();
		}
	}

	// Simple remaining helpers moved from header
	void AbstractControl::setScene(Scene* scene) {
		scene_ = scene;
	}

	Scene* AbstractControl::getScene() const {
		return scene_;
	}

	void AbstractControl::setParent(AbstractControl* parent) {
		parent_ = parent;
	}

	AbstractControl* AbstractControl::getParent() const {
		return parent_;
	}

	const ui::Rect& AbstractControl::getBounds() const {
		return bounds_;
	}

	void AbstractControl::setFillColor(const ui::Color& color) {
		fillColor_ = color;
	}

	const ui::Color& AbstractControl::getFillColor() const {
		return fillColor_;
	}

	void AbstractControl::setStrokeColor(const ui::Color& color) {
		strokeColor_ = color;
	}

	const ui::Color& AbstractControl::getStrokeColor() const {
		return strokeColor_;
	}

	void AbstractControl::setStrokeWidth(const ui::Edges& width) {
		strokeWidth_ = width;
	}

	const ui::Edges& AbstractControl::getStrokeWidth() const {
		return strokeWidth_;
	}

	void AbstractControl::setCornerRadius(float radius) {
		cornerRadius_ = radius;
	}

	float AbstractControl::getCornerRadius() const {
		return cornerRadius_;
	}

	void AbstractControl::setBackgroundImage(const std::string& path) {
		backgroundImage_ = path;
	}

	const std::string& AbstractControl::getBackgroundImage() const {
		return backgroundImage_;
	}

	void AbstractControl::setVisible(bool visible) {
		visible_ = visible;
		auto scene = getScene();
		if (scene)
		{
			ResolvedNode* rn = ControlFactory::getInstance()->FindNodeByUid(getUid());
			if (rn)
			{
				rn->finalStyle.visible = visible_;
				rn->markDirty(ldt::DirtyFlag::Style);
			}
		}
	}

	bool AbstractControl::isVisible() const {
		return visible_;
	}

	void AbstractControl::setEnabled(bool enabled) {
		if (enabled_ == enabled) return;
		enabled_ = enabled;

		auto scene = getScene();
		if (scene) {
			std::string uid = getUid();
			if (!uid.empty()) {
				ResolvedNode* rn = ControlFactory::getInstance()->FindNodeByUid(uid);
				if (rn) {
					if (enabled_) {
						rn->removeUIState(UIState::Disabled);
					} else {
						rn->addUIState(UIState::Disabled);
						// Force clear other interaction states
						rn->removeUIState(UIState::Hover);
						rn->removeUIState(UIState::Active);
						rn->removeUIState(UIState::Focus);
					}
				}
			}
			// If we just got disabled and had focus, we should probably lose it
			if (!enabled_ && isFocused()) {
				// Scene handles focus changes, but we check if we need to request blur
				// Actually Scene logic usually polls or handles events.
				// We can rely on Scene or just clear our flag if scene doesn't manage strict focus list
				// But scene_ usually manages the focused control pointer.
			}
		}
	}

	bool AbstractControl::isEnabled() const {
		return enabled_;
	}

	void AbstractControl::setId(const std::string& id) {
		id_ = id;
	}

	const std::string& AbstractControl::getId() const {
		return id_;
	}

	void AbstractControl::setUid(const std::string& uid) {
		uid_ = uid;
	}

	const std::string& AbstractControl::getUid() const {
		return uid_;
	}

	void AbstractControl::setClassList(const std::vector<std::string>& cls, bool forceLayout) {
		classes_ = cls;
		//更新style
		auto scene = getScene();
		if (scene)
		{
			ResolvedNode* rn = ControlFactory::getInstance()->FindNodeByUid(getUid());
			if (!rn || !rn->astNode) return;
			rn->astNode->setClassList(classes_);
			rn->recompileStyleAndLayout(scene->getDocumentRuntime());
			auto flag = ldt::DirtyFlag::Style;
			if (forceLayout)
			{
				flag |= ldt::DirtyFlag::Layout;
			}
			rn->markDirty(flag);
		}
	}

	void AbstractControl::addClass(const std::string& c, bool forceLayout) {
		if (c.empty() || hasClass(c)) return;
		auto next = classes_;
		next.push_back(c);
		setClassList(next, forceLayout);
	}

	void AbstractControl::removeClass(const std::string& c, bool forceLayout) {
		if (c.empty()) return;
		auto next = classes_;
		auto it = std::remove(next.begin(), next.end(), c);
		if (it == next.end()) return;
		next.erase(it, next.end());
		setClassList(next, forceLayout);
	}

	const std::vector<std::string>& AbstractControl::getClassList() const {
		return classes_;
	}

	bool AbstractControl::hasClass(const std::string& c) const {
		for (const auto& it : classes_) if (it == c) return true;
		return false;
	}

	bool AbstractControl::handleLocalHitTest(ControlLocalPointDp point) const {
		return point.x.value >= 0.0f && point.x.value <= bounds_.width
			&& point.y.value >= 0.0f && point.y.value <= bounds_.height;
	}
	bool AbstractControl::handleLogicalHitTest(LogicalPointDp point) const { return handleLocalHitTest(toLocalPoint(point)); }
	bool AbstractControl::handleLocalMouseMove(ControlLocalPointDp point) { (void)point; return false; }
	bool AbstractControl::handleLocalMouseButton(ControlLocalPointDp point, int button, int action) { (void)point; (void)button; (void)action; return false; }
	bool AbstractControl::onLocalMouseMove(ControlLocalPointDp point) { return handleLocalMouseMove(point); }
	bool AbstractControl::onLocalMouseButton(ControlLocalPointDp point, int button, int action) { return handleLocalMouseButton(point, button, action); }
	ControlLocalPointDp AbstractControl::toLocalPoint(LogicalPointDp point) const {
		// Compute the visual screen position of this control, accounting for
		// ancestor scroll offsets (which shift children upward) and visual
		// offsets (drag transforms on ancestors and self).
		float visualScreenX = bounds_.x + visualOffsetX_;
		float visualScreenY = bounds_.y + visualOffsetY_;
		auto* anc = parent_;
		while (anc) {
			visualScreenX += anc->getVisualOffsetX() - anc->getScrollX();
			visualScreenY += anc->getVisualOffsetY() - anc->getScrollY();
			anc = anc->getParent();
		}
		return {
			{ point.x.value - visualScreenX },
			{ point.y.value - visualScreenY }
		};
	}
	bool AbstractControl::hitTestLocal(ControlLocalPointDp point) const { return handleLocalHitTest(point); }
	bool AbstractControl::hitTestPoint(LogicalPointDp point) const { return handleLogicalHitTest(point); }
	bool AbstractControl::onKey(int key, int scancode, int action, int mods) { (void)key; (void)scancode; (void)action; (void)mods; return false; }
	bool AbstractControl::onChar(unsigned int codepoint) { (void)codepoint; return false; }
	bool AbstractControl::handleLocalScroll(ControlLocalScrollDeltaDp delta) { (void)delta; return false; }
	bool AbstractControl::onLocalScroll(ControlLocalScrollDeltaDp delta) { return handleLocalScroll(delta); }

	void AbstractControl::onFocus() { focused_ = true; }
	void AbstractControl::onBlur() { focused_ = false; }
	bool AbstractControl::isFocused() const { return focused_; }

	void AbstractControl::onHover(bool isHovered) { (void)isHovered; }
	void AbstractControl::onActive(bool isActive) { (void)isActive; }

	bool AbstractControl::wantsMouseCapture() const { return false; }

	void AbstractControl::onDragStart()
	{
		std::string uid = getUid();
		if (!uid.empty()) {
			ResolvedNode* rn = ControlFactory::getInstance()->FindNodeByUid(uid);
			if (rn) {
				// 如果是居中对齐，则在拖拽开始时将其转换为 TopLeft
				// (这也是为了让 visualOffset 的参考系从 TopLeft 开始，更为直观)
				if (rn->finalStyle.anchor == Anchor::Center && rn->parent) {
					rn->finalStyle.offsetX += (rn->parent->layout.computedWidth - rn->layout.computedWidth) * 0.5f;
					rn->finalStyle.offsetY += (rn->parent->layout.computedHeight - rn->layout.computedHeight) * 0.5f;
					rn->finalStyle.anchor = Anchor::TopLeft;
					
					// 强制同步一次静态布局数据，这一步无法避免，但只会发生一次
					rn->markDirty(DirtyFlag::Layout);
				}
			}
		}
	}

	void AbstractControl::handleLogicalDragMove(LogicalDragOffsetDp delta)
	{
		// 拖拽过程：仅使用视觉偏移，不触发布局引擎
		setVisualOffset(delta.dx.value, delta.dy.value);
	}

	void AbstractControl::onLogicalDragMove(LogicalDragOffsetDp delta)
	{
		handleLogicalDragMove(delta);
	}

	void AbstractControl::onDrop()
	{
		// 拖拽结束：将视觉偏移同步到引擎的 Source of Truth (Style)
		std::string uid = getUid();
		if (!uid.empty() && (visualOffsetX_ != 0 || visualOffsetY_ != 0)) {
			ResolvedNode* rn = ControlFactory::getInstance()->FindNodeByUid(uid);
			if (rn) {
				rn->finalStyle.offsetX += visualOffsetX_;
				rn->finalStyle.offsetY += visualOffsetY_;
				
				// 重置视觉偏移
				setVisualOffset(0, 0);

				// 同步完成，由于位置变化影响到了绝对布局，触发一次布局重算
				rn->markDirty(DirtyFlag::Layout);
			}
		}
	}

	void AbstractControl::setVisualOffset(float x, float y) {
		if (ldt::floatNotEqual(visualOffsetX_, x) || ldt::floatNotEqual(visualOffsetY_, y)) {
			visualOffsetX_ = x;
			visualOffsetY_ = y;
			invalidate(); // 请求重绘
		}
	}

	float AbstractControl::getScrollX() const { return scrollX_; }
	float AbstractControl::getScrollY() const { return scrollY_; }

	float AbstractControl::getScrollWidth() const { return scrollWidth_; }
	float AbstractControl::getScrollHeight() const { return scrollHeight_; }

	void AbstractControl::setViewportSize(float w, float h) 
	{
		viewportWidth_ = w;
		viewportHeight_ = h;
	}
	float AbstractControl::getViewportWidth() const { return viewportWidth_; }
	float AbstractControl::getViewportHeight() const { return viewportHeight_; }
	bool AbstractControl::hasViewport() const { return viewportWidth_ > 0.0f && viewportHeight_ > 0.0f; }

	void AbstractControl::setPadding(float left, float top, float right, float bottom) {
		paddingLeft_ = left;
		paddingTop_ = top;
		paddingRight_ = right;
		paddingBottom_ = bottom;
	}

	void AbstractControl::markLayerDirty() {
		isLayerDirty_ = true;
		if (parent_) {
			parent_->markLayerDirty();
		}
	}

} // namespace ldt
