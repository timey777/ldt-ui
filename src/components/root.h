#pragma once

#include "panel.h"

namespace ldt {

/**
 * @brief 根控件
 * 
 * 一个容器控件，可以包含其他控件，并绘制背景和边框
 */
class LDT_API Root : public Panel {
public:
    // 禁止外部直接构造，使用 ControlFactory 创建
protected:
    Root() = default;
    friend class ControlFactory;

public:
    std::string getTypeName() const override { return "Root"; }

};

} // namespace ldt

