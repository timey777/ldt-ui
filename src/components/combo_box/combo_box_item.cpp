#include "combo_box_item.h"
#include "render/display_list.h"

namespace ldt {

ComboBoxItem::ComboBoxItem(const std::string& text, int index, std::function<void(int)> onClick)
    : text_(text), index_(index), onClick_(onClick) {}

void ComboBoxItem::onRender(DisplayList& displayList, const ui::Rect& clip) const {
    
    // Draw background - 选中 > 悬浮 > 默认
    ui::Color bg;
    if (isSelected_) {
        bg = ui::Color(0.2f, 0.5f, 0.9f, 1.0f);  // 蓝色表示选中
    } else if (isHovered_) {
        bg = ui::Color(0.95f, 0.95f, 0.95f, 1.0f);  // 浅灰色表示悬浮
    } else {
        bg = ui::Color(1, 1, 1, 1);  // 白色背景
    }
    displayList.addRect(bounds_, bg);
    
    // Draw text - 选中时文字为白色，否则为黑色
    ui::Color textColor = isSelected_ ? ui::Color(1, 1, 1, 1) : ui::Color(0, 0, 0, 1);
    ui::Rect contentRect = bounds_;
    contentRect.x += 10;
    contentRect.width -= 20;
    displayList.addText(text_, contentRect, textColor, 14.0f);
}

bool ComboBoxItem::handleLocalMouseMove(ControlLocalPointDp point) {
    if (!visible_) return false;
    bool inside = hitTestLocal(point);
    if (isHovered_ != inside) {
        isHovered_ = inside;
        invalidate();  // 触发重绘
    }
    return inside;
}

bool ComboBoxItem::handleLocalMouseButton(ControlLocalPointDp point, int button, int action) {
    if (!visible_) return false;
    if (hitTestLocal(point)) {
        if (action == 0 && button == 0) { // Release
            if (onClick_) onClick_(index_);
            return true;
        }
        return true; // Consume press
    }
    return false;
}

} // namespace ldt
