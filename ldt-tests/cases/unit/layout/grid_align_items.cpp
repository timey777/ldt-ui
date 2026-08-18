#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

// grid 的 align-items 特性（等价 CSS Grid 的块轴对齐）：
//   - 默认 stretch：auto 高度的子项被拉满行高（原行为）
//   - align-items: center / flex-end：子项保留内容高度，在行内垂直居中 / 贴底
//   - 用于 dashboard.ldt 表格单元格内容的垂直居中

// 默认（align-items 未设置 = stretch）：auto 高度子项拉满行高
TEST_CASE("layout/grid_align_items_default_stretch") {
    TestEnv env;
    env.load(
        "@style {"
        " .grid { width:400; height:100; }"
        "}"
        "@layout {"
        " .grid { display:grid; grid-template-columns:200px 200px; grid-template-rows:100px; }"
        "}"
        "panel:root {"
        " panel:grid(class=\"grid\") {"
        "  panel:a(),"
        "  panel:b()"
        " }"
        "}");
    auto* grid = env.findById("grid");
    auto* a = env.findById("a");
    EXPECT_NOT_NULL(grid);
    EXPECT_NOT_NULL(a);

    // auto 高度 → 被 stretch 拉满行高 100，顶部对齐
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().height, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(a->layout.getBorderBox().y - grid->layout.getBorderBox().y, 0.0f, 0.5f);
}

// align-items: center：保留内容高度，行内垂直居中
TEST_CASE("layout/grid_align_items_center") {
    TestEnv env;
    env.load(
        "@style {"
        " .grid  { width:400; height:100; }"
        " .tall  { width:100; height:60; }"
        " .small { width:100; height:30; }"
        "}"
        "@layout {"
        " .grid { display:grid; grid-template-columns:200px 200px; grid-template-rows:100px; align-items:center; }"
        "}"
        "panel:root {"
        " panel:grid(class=\"grid\") {"
        "  panel:tall(class=\"tall\"),"
        "  panel:small(class=\"small\")"
        " }"
        "}");
    auto* grid = env.findById("grid");
    auto* tall = env.findById("tall");
    auto* small = env.findById("small");
    EXPECT_NOT_NULL(grid);
    EXPECT_NOT_NULL(tall);
    EXPECT_NOT_NULL(small);

    // 不被 stretch：保留各自高度
    EXPECT_FLOAT_EQ(tall->layout.getBorderBox().height, 60.0f, 0.5f);
    EXPECT_FLOAT_EQ(small->layout.getBorderBox().height, 30.0f, 0.5f);
    // 行内居中：top = (100-60)/2 = 20；top = (100-30)/2 = 35
    EXPECT_FLOAT_EQ(tall->layout.getBorderBox().y - grid->layout.getBorderBox().y, 20.0f, 0.5f);
    EXPECT_FLOAT_EQ(small->layout.getBorderBox().y - grid->layout.getBorderBox().y, 35.0f, 0.5f);
}

// align-items: flex-end：保留内容高度，行内贴底
TEST_CASE("layout/grid_align_items_flex_end") {
    TestEnv env;
    env.load(
        "@style {"
        " .grid  { width:400; height:100; }"
        " .tall  { width:100; height:60; }"
        " .small { width:100; height:30; }"
        "}"
        "@layout {"
        " .grid { display:grid; grid-template-columns:200px 200px; grid-template-rows:100px; align-items:flex-end; }"
        "}"
        "panel:root {"
        " panel:grid(class=\"grid\") {"
        "  panel:tall(class=\"tall\"),"
        "  panel:small(class=\"small\")"
        " }"
        "}");
    auto* grid = env.findById("grid");
    auto* tall = env.findById("tall");
    auto* small = env.findById("small");
    EXPECT_NOT_NULL(grid);
    EXPECT_NOT_NULL(tall);
    EXPECT_NOT_NULL(small);

    // 贴底：top = 100-60 = 40；top = 100-30 = 70
    EXPECT_FLOAT_EQ(tall->layout.getBorderBox().y - grid->layout.getBorderBox().y, 40.0f, 0.5f);
    EXPECT_FLOAT_EQ(small->layout.getBorderBox().y - grid->layout.getBorderBox().y, 70.0f, 0.5f);
}

// dashboard.ldt 表格场景回归：行高由最高单元格决定，较短单元格内容垂直居中。
//   行高 68；badge 单元格(高48) 居中偏移 (68-48)/2=10，badge 再 +padding-top14 → 距行顶 24
//   btn 单元格(高58) 居中偏移 (68-58)/2=5，btn 再 +padding-top14 → 距行顶 19
TEST_CASE("layout/grid_align_items_table_cell_scenario") {
    TestEnv env;
    env.load(
        "@style {"
        " .tr    { width:1050; }"
        " .td    { padding:14px 24px; }"
        " .avatar{ width:40; height:40; }"
        " .badge { width:80; height:20; }"
        " .btn   { width:100; height:30; }"
        "}"
        "@layout {"
        " .tr  { display:grid; grid-template-columns:350px 350px 350px; align-items:center; }"
        "}"
        "panel:root {"
        " panel:tr(class=\"tr\") {"
        "  panel:cell1(class=\"td\") {"
        "   panel:avatar(class=\"avatar\")"
        "  },"
        "  panel:cell2(class=\"td\") {"
        "   panel:badge(class=\"badge\")"
        "  },"
        "  panel:cell3(class=\"td\") {"
        "   panel:btn(class=\"btn\")"
        "  }"
        " }"
        "}");
    auto* tr = env.findById("tr");
    auto* cell2 = env.findById("cell2");
    auto* cell3 = env.findById("cell3");
    auto* badge = env.findById("badge");
    auto* btn = env.findById("btn");
    EXPECT_NOT_NULL(tr);
    EXPECT_NOT_NULL(cell2);
    EXPECT_NOT_NULL(cell3);
    EXPECT_NOT_NULL(badge);
    EXPECT_NOT_NULL(btn);

    // 行高 = 最高单元格（avatar 40 + padding 28 = 68）
    EXPECT_FLOAT_EQ(cell2->layout.getBorderBox().height, 48.0f, 0.5f);   // badge 单元格不被拉伸
    EXPECT_FLOAT_EQ(cell3->layout.getBorderBox().height, 58.0f, 0.5f);   // btn 单元格不被拉伸
    EXPECT_FLOAT_EQ(tr->layout.getBorderBox().height, 68.0f, 0.5f);

    // badge 距行顶 = 居中偏移(10) + padding-top(14) = 24；btn 距行顶 = 5 + 14 = 19
    EXPECT_FLOAT_EQ(badge->layout.getBorderBox().y - tr->layout.getBorderBox().y, 24.0f, 0.5f);
    EXPECT_FLOAT_EQ(btn->layout.getBorderBox().y - tr->layout.getBorderBox().y, 19.0f, 0.5f);
}
