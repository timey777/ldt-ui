#include "text.h"

#include "control_factory.h"
#include "engine/core/attribute.h"
#include "engine/core/resolved_node.h"

namespace ldt {

void Text::OnSyncFromResolvedNode(const ResolvedNode& rn) {
    // 字体/文本属性：运行时样式（hover/active 等）变化也需刷新，不能只在创建时设置
    setTextColor(rn.finalStyle.textColor);
    setFontSize(rn.finalStyle.fontSize);
    setBold(rn.finalStyle.fontWeight == ldt::FontWeight::Bold || rn.finalStyle.fontWeight == ldt::FontWeight::W700);
    setItalic(rn.finalStyle.fontStyle == ldt::FontStyle::Italic || rn.finalStyle.fontStyle == ldt::FontStyle::Oblique);
    setLineHeight(rn.finalStyle.lineHeight);
    setFontFamily(rn.finalStyle.fontFamily);

    // 布局派生属性
    setWrap(rn.layout.wrap);
    setAlignment(rn.finalStyle.textAlign);
    setLayoutWidth(rn.layout.getBorderBox().width);
    // padding 已在 AbstractControl::syncFromResolvedNode 通用层统一同步
}

void Text::setText(const std::string& text) {
    if (text_ == text) {
        return;
    }

    text_ = text;
    layoutDirty_ = true;
    invalidate();

    const std::string& uid = getUid();
    if (uid.empty()) {
        return;
    }

    auto* factory = ControlFactory::getInstance();
    if (!factory) {
        return;
    }

    auto* resolvedNode = factory->FindNodeByUid(uid);
    if (!resolvedNode) {
        return;
    }

    if (resolvedNode->astNode) {
        resolvedNode->astNode->setAttribute("value", Attribute(text_));
    }

    resolvedNode->markDirty(DirtyFlag::Layout | DirtyFlag::Paint);
}

} // namespace ldt