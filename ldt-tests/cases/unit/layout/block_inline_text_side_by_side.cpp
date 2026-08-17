#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

// 验证：父容器为 block（默认）时，inline 子元素（text）水平并排、同一行、不重叠
TEST_CASE("layout/block_inline_text_side_by_side") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .t { font-size:14; }\n"
        "}\n"
        "panel:container {\n"
        "    text:t1(class=\"t\", value=\"Hello\")\n"
        "    text:t2(class=\"t\", value=\"World\")\n"
        "}\n";

    auto* root = env.load(ldt);
    EXPECT_NOT_NULL(root);

    auto* t1 = env.findById("t1");
    auto* t2 = env.findById("t2");
    EXPECT_NOT_NULL(t1);
    EXPECT_NOT_NULL(t2);

    float x1 = t1->layout.getBorderBox().x;
    float x2 = t2->layout.getBorderBox().x;
    float y1 = t1->layout.getBorderBox().y;
    float y2 = t2->layout.getBorderBox().y;

    // 同一行
    EXPECT_FLOAT_EQ(y1, y2, 0.5f);
    // 水平并排：t2 在 t1 右侧
    EXPECT_LT(x1, x2);
    // 不重叠：t2.x >= t1.x + t1.width
    EXPECT_GE(x2, x1 + t1->layout.getBorderBox().width - 0.5f);
}

// 验证：block 容器里混合排列——block 子元素独占一行（垂直堆叠），
// inline 子元素水平并排；inline 行在 block 行之后另起一行
TEST_CASE("layout/block_mixed_block_and_inline_children") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .b { width:80; height:20; }\n"
        "    .t { font-size:14; }\n"
        "}\n"
        "panel:container {\n"
        "    panel:blockChild(class=\"b\")\n"
        "    text:t1(class=\"t\", value=\"Hello\")\n"
        "    text:t2(class=\"t\", value=\"World\")\n"
        "}\n";

    auto* root = env.load(ldt);
    EXPECT_NOT_NULL(root);

    auto* blockChild = env.findById("blockChild");
    auto* t1 = env.findById("t1");
    auto* t2 = env.findById("t2");
    EXPECT_NOT_NULL(blockChild);
    EXPECT_NOT_NULL(t1);
    EXPECT_NOT_NULL(t2);

    float by = blockChild->layout.getBorderBox().y;
    float y1 = t1->layout.getBorderBox().y;
    float y2 = t2->layout.getBorderBox().y;

    // block 子元素独占一行：inline 行在它下方
    EXPECT_GE(y1, by + blockChild->layout.getBorderBox().height - 0.5f);
    // inline 子元素同一行、水平并排
    EXPECT_FLOAT_EQ(y1, y2, 0.5f);
    EXPECT_LT(t1->layout.getBorderBox().x, t2->layout.getBorderBox().x);
}
