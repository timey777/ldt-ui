#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

TEST_CASE("layout/flex_basic") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .container { width:400; height:100; }\n"
        "    .item { width:100; height:50; }\n"
        "}\n"
        "@layout {\n"
        "    .container { display:flex; flex-direction:row; }\n"
        "}\n"
        "panel:container(class=\"container\") {\n"
        "    panel:item1(class=\"item\")\n"
        "    panel:item2(class=\"item\")\n"
        "    panel:item3(class=\"item\")\n"
        "}\n";

    auto* root = env.load(ldt);
    EXPECT_NOT_NULL(root);

    auto* item1 = env.findById("item1");
    auto* item2 = env.findById("item2");
    auto* item3 = env.findById("item3");
    EXPECT_NOT_NULL(item1);
    EXPECT_NOT_NULL(item2);
    EXPECT_NOT_NULL(item3);

    // flex-direction:row → 子节点应水平排列，x 坐标递增
    float x1 = item1->layout.getBorderBox().x;
    float x2 = item2->layout.getBorderBox().x;
    float x3 = item3->layout.getBorderBox().x;
    EXPECT_LT(x1, x2);
    EXPECT_LT(x2, x3);

    // 每个 item 宽度应保持 100
    EXPECT_FLOAT_EQ(item1->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item2->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item3->layout.getBorderBox().width, 100.0f, 0.5f);
}
