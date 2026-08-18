#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

// ComboBox 的 trigger/popup 是运行时 addChild 加入的，不在 AST flow children 里，
// 布局引擎无法靠子项推断内在尺寸。修复前 auto 宽高会塌缩为 0×0（完全不可见）。
//
// 修复（box_model_engine.cpp measureContent 的 combobox 分支）：
//   宽 = 最长 option 文本宽度 + 左侧文字留白(8) + 右侧箭头区(24)
//   高 = 单行行高
// 仅作用于 auto 轴；显式 width/height 优先。
TEST_CASE("ui/combobox_auto_size_from_longest_option") {
    TestEnv env;
    std::string ldt =
        "combobox:cmb(options=\"File,TCP,UDP,Serial\")\n";

    env.load(ldt, 800.0f, 600.0f);

    auto* cmb = env.findById("cmb");
    EXPECT_NOT_NULL(cmb);

    // stub 测量：charWidth=8, lineHeight=20（默认字号 14，缩放系数 1）
    // 最长项 "Serial" = 6 字符 → 文本宽 48；+8(左) +24(箭头) = 80
    EXPECT_FLOAT_EQ(cmb->layout.computedWidth, 80.0f, 0.5f);
    // 高 = 行高 20
    EXPECT_FLOAT_EQ(cmb->layout.computedHeight, 20.0f, 0.5f);
}

// 显式 width/height 优先于内在尺寸：auto 内在测量只作用于未指定尺寸的轴
TEST_CASE("ui/combobox_explicit_size_wins_over_intrinsic") {
    TestEnv env;
    std::string ldt =
        "combobox:cmb(options=\"File,TCP,UDP,Serial\", width=\"120px\", height=\"30px\")\n";

    env.load(ldt, 800.0f, 600.0f);

    auto* cmb = env.findById("cmb");
    EXPECT_NOT_NULL(cmb);
    EXPECT_FLOAT_EQ(cmb->layout.computedWidth, 120.0f, 0.5f);
    EXPECT_FLOAT_EQ(cmb->layout.computedHeight, 30.0f, 0.5f);
}
