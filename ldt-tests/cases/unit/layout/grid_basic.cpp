#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

// 根节点会被引擎强制为视口尺寸（BoxModelEngine::process 覆写 finalStyle），
// 所以直接以 grid 容器为根，用 viewport 控制容器尺寸。

// 三列 1fr：均分容器宽度，项被 stretch 填满各自 cell
TEST_CASE("layout/grid_fr_even_split") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .grid { width:100%; height:100%; }\n"
        "}\n"
        "@layout {\n"
        "    .grid { display:grid; grid-template-columns:1fr 1fr 1fr; }\n"
        "}\n"
        "panel:grid(class=\"grid\") {\n"
        "    panel:item1(class=\"item\")\n"
        "    panel:item2(class=\"item\")\n"
        "    panel:item3(class=\"item\")\n"
        "}\n";

    env.load(ldt, 300.0f, 100.0f);

    auto* item1 = env.findById("item1");
    auto* item2 = env.findById("item2");
    auto* item3 = env.findById("item3");
    EXPECT_NOT_NULL(item1);
    EXPECT_NOT_NULL(item2);
    EXPECT_NOT_NULL(item3);

    // 三列均分 300 → 每列 100
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item2->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item3->layout.getBorderBox().width, 100.0f, 0.5f);

    // 横向依次排开
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().x, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(item2->layout.getBorderBox().x, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item3->layout.getBorderBox().x, 200.0f, 0.5f);
}

// 固定 px + fr 混合：固定轨道先占，剩余给 fr
TEST_CASE("layout/grid_px_fr_mix") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .grid { width:100%; height:100%; }\n"
        "}\n"
        "@layout {\n"
        "    .grid { display:grid; grid-template-columns:100px 1fr 1fr; }\n"
        "}\n"
        "panel:grid(class=\"grid\") {\n"
        "    panel:item1(class=\"item\")\n"
        "    panel:item2(class=\"item\")\n"
        "    panel:item3(class=\"item\")\n"
        "}\n";

    env.load(ldt, 300.0f, 100.0f);

    auto* item1 = env.findById("item1");
    auto* item2 = env.findById("item2");
    auto* item3 = env.findById("item3");
    EXPECT_NOT_NULL(item1);
    EXPECT_NOT_NULL(item2);
    EXPECT_NOT_NULL(item3);

    // 300 = 100px + 剩余 200 均分给两个 1fr
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item2->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item3->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().x, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(item2->layout.getBorderBox().x, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item3->layout.getBorderBox().x, 200.0f, 0.5f);
}

// gap：剩余空间要减去 gap 再分给 fr
TEST_CASE("layout/grid_gap") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .grid { width:100%; height:100%; }\n"
        "}\n"
        "@layout {\n"
        "    .grid { display:grid; grid-template-columns:1fr 1fr; gap:10px; }\n"
        "}\n"
        "panel:grid(class=\"grid\") {\n"
        "    panel:item1(class=\"item\")\n"
        "    panel:item2(class=\"item\")\n"
        "}\n";

    env.load(ldt, 210.0f, 100.0f);

    auto* item1 = env.findById("item1");
    auto* item2 = env.findById("item2");
    EXPECT_NOT_NULL(item1);
    EXPECT_NOT_NULL(item2);

    // 210 = 100 + gap(10) + 100
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item2->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().x, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(item2->layout.getBorderBox().x, 110.0f, 0.5f);
}

// 隐式行：项数超过列数 → 自动换到下一行（auto 行高 = 内容高度）
TEST_CASE("layout/grid_auto_flow_wraps_to_new_row") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .grid { width:100%; height:100%; }\n"
        "    .item { height:40px; }\n"
        "}\n"
        "@layout {\n"
        "    .grid { display:grid; grid-template-columns:1fr 1fr; }\n"
        "}\n"
        "panel:grid(class=\"grid\") {\n"
        "    panel:item1(class=\"item\")\n"
        "    panel:item2(class=\"item\")\n"
        "    panel:item3(class=\"item\")\n"
        "    panel:item4(class=\"item\")\n"
        "}\n";

    env.load(ldt, 200.0f, 200.0f);

    auto* item1 = env.findById("item1");
    auto* item3 = env.findById("item3");
    auto* item4 = env.findById("item4");
    EXPECT_NOT_NULL(item1);
    EXPECT_NOT_NULL(item3);
    EXPECT_NOT_NULL(item4);

    // 第一行：y=0；第二行：y = row0 高度(40) + gap(0) = 40
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().y, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(item3->layout.getBorderBox().y, 40.0f, 0.5f);
    // 宽度均分 200 → 100
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item4->layout.getBorderBox().width, 100.0f, 0.5f);
}
