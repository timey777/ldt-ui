#pragma once

#include "abstract_control.h"
#include <memory>

namespace ldt {

// Forward declare AbstractControl usage; ScrollbarControl works with any control exposing scroll API
class AbstractControl;

class ScrollbarControl : public AbstractControl {
public:
    static constexpr float kScrollbarThickness = 12.0f; // Standard scrollbar size
    static constexpr float kMinThumbSize = 32.0f; // Keep thumb usable when content is huge

    enum class Orientation { Vertical, Horizontal };

protected:
    // Protected so only ControlFactory (friend) can create instances
    ScrollbarControl(Orientation o) : orient_(o) {}
    friend class ControlFactory;

public:
    ~ScrollbarControl() override = default;
    std::string getTypeName() const override { return "ScrollbarControl"; }

protected:
    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    bool onParentLocalMouseMove(ParentLocalPointDp point);
    bool onParentLocalMouseButton(ParentLocalPointDp point, int button, int action);
    void onActive(bool isActive) override { if (!isActive) dragging_ = false; }

protected:
    virtual bool handleParentLocalMouseMove(ParentLocalPointDp point);
    virtual bool handleParentLocalMouseButton(ParentLocalPointDp point, int button, int action);

private:
    Orientation orient_;
    bool dragging_ = false;
    float dragStartPos_ = 0.0f;
    float dragStartScroll_ = 0.0f;
};

} // namespace ldt
