#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

// align-self 特性（子项级，覆盖父容器 align-items，等价 CSS）：
//   - 默认 auto：跟随父容器 align-items
//   - center / flex-end / flex-start / stretch / baseline：本子项单独指定
// flex 和 grid 均支持。

// ── flex：容器默认 stretch，子项 b 用 align-self:center ──
TEST_CASE("layout/align_self_flex_center") {
    TestEnv env;
    env.load(
        "@style {"
        " .flex { width:400; height:100; }"
        " .a { width:100; }"
        " .b { width:100; height:40; }"
        "}"
        "@layout {"
        " .flex { display:flex; flex-direction:row; }"
        " .b { align-self:center; }"
        "}"
        "panel:root {"
        " panel:flex(class=\"flex\") {"
        "  panel:a(class=\"a\"),"
        "  panel:b(class=\"b\")"
        " }"
        "}");
    auto* flex = env.findById("flex");
    auto* a = env.findById("a");
    auto* b = env.findById("b");
    EXPECT_NOT_NULL(flex);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);

    // a（auto）→ 继承父 stretch：拉满高度 100，顶部
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().height, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().y - flex->layout.getBorderBox().y, 0.0f, 0.5f);
    // b（align-self:center）→ 不被拉伸，交叉轴居中：(100-40)/2 = 30
    EXPECT_FLOAT_EQ(b->layout.getBorderBox().height, 40.0f, 0.5f);
    EXPECT_FLOAT_EQ(b->layout.getBorderBox().y - flex->layout.getBorderBox().y, 30.0f, 0.5f);
}

// ── flex：容器 align-items:center，子项 b 用 align-self:flex-end 贴底 ──
TEST_CASE("layout/align_self_flex_end") {
    TestEnv env;
    env.load(
        "@style {"
        " .flex { width:400; height:100; }"
        " .a { width:100; height:60; }"
        " .b { width:100; height:40; }"
        "}"
        "@layout {"
        " .flex { display:flex; flex-direction:row; align-items:center; }"
        " .b { align-self:flex-end; }"
        "}"
        "panel:root {"
        " panel:flex(class=\"flex\") {"
        "  panel:a(class=\"a\"),"
        "  panel:b(class=\"b\")"
        " }"
        "}");
    auto* flex = env.findById("flex");
    auto* a = env.findById("a");
    auto* b = env.findById("b");
    EXPECT_NOT_NULL(flex);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);

    // a 跟随父 align-items:center → (100-60)/2 = 20
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().y - flex->layout.getBorderBox().y, 20.0f, 0.5f);
    // b 用 align-self:flex-end → 100-40 = 60
    EXPECT_FLOAT_EQ(b->layout.getBorderBox().y - flex->layout.getBorderBox().y, 60.0f, 0.5f);
}

// ── grid：容器默认 stretch，子项 b 用 align-self:center ──
TEST_CASE("layout/align_self_grid_center") {
    TestEnv env;
    env.load(
        "@style {"
        " .grid { width:400; height:100; }"
        " .a { width:100; }"
        " .b { width:100; height:40; }"
        "}"
        "@layout {"
        " .grid { display:grid; grid-template-columns:200px 200px; grid-template-rows:100px; }"
        " .b { align-self:center; }"
        "}"
        "panel:root {"
        " panel:grid(class=\"grid\") {"
        "  panel:a(class=\"a\"),"
        "  panel:b(class=\"b\")"
        " }"
        "}");
    auto* grid = env.findById("grid");
    auto* a = env.findById("a");
    auto* b = env.findById("b");
    EXPECT_NOT_NULL(grid);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);

    // a（auto）→ 继承父 stretch：拉满行高 100，顶部
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().height, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().y - grid->layout.getBorderBox().y, 0.0f, 0.5f);
    // b（align-self:center）→ 不被拉伸，行内居中：(100-40)/2 = 30
    EXPECT_FLOAT_EQ(b->layout.getBorderBox().height, 40.0f, 0.5f);
    EXPECT_FLOAT_EQ(b->layout.getBorderBox().y - grid->layout.getBorderBox().y, 30.0f, 0.5f);
}

// ── grid：容器 align-items:center，子项 b 用 align-self:flex-end 贴底 ──
TEST_CASE("layout/align_self_grid_flex_end") {
    TestEnv env;
    env.load(
        "@style {"
        " .grid { width:400; height:100; }"
        " .a { width:100; height:60; }"
        " .b { width:100; height:40; }"
        "}"
        "@layout {"
        " .grid { display:grid; grid-template-columns:200px 200px; grid-template-rows:100px; align-items:center; }"
        " .b { align-self:flex-end; }"
        "}"
        "panel:root {"
        " panel:grid(class=\"grid\") {"
        "  panel:a(class=\"a\"),"
        "  panel:b(class=\"b\")"
        " }"
        "}");
    auto* grid = env.findById("grid");
    auto* a = env.findById("a");
    auto* b = env.findById("b");
    EXPECT_NOT_NULL(grid);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);

    // a 跟随父 align-items:center → (100-60)/2 = 20
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().y - grid->layout.getBorderBox().y, 20.0f, 0.5f);
    // b 用 align-self:flex-end → 100-40 = 60
    EXPECT_FLOAT_EQ(b->layout.getBorderBox().y - grid->layout.getBorderBox().y, 60.0f, 0.5f);
}

// ── grid：align-self 默认 auto，跟随父 align-items:center ──
TEST_CASE("layout/align_self_grid_auto_follows_parent") {
    TestEnv env;
    env.load(
        "@style {"
        " .grid { width:400; height:100; }"
        " .a { width:100; height:60; }"
        " .b { width:100; height:40; }"
        "}"
        "@layout {"
        " .grid { display:grid; grid-template-columns:200px 200px; grid-template-rows:100px; align-items:center; }"
        "}"
        "panel:root {"
        " panel:grid(class=\"grid\") {"
        "  panel:a(class=\"a\"),"
        "  panel:b(class=\"b\")"
        " }"
        "}");
    auto* grid = env.findById("grid");
    auto* a = env.findById("a");
    auto* b = env.findById("b");
    EXPECT_NOT_NULL(grid);
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);

    // 都跟随父 align-items:center
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().y - grid->layout.getBorderBox().y, 20.0f, 0.5f);
    EXPECT_FLOAT_EQ(b->layout.getBorderBox().y - grid->layout.getBorderBox().y, 30.0f, 0.5f);
}
