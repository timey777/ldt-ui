#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

TEST_CASE("layout/flex_shrink") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .container { width:300; height:100; }\n"
        "    .item-a { width:200; height:50; }\n"
        "    .item-b { width:200; height:50; }\n"
        "}\n"
        "@layout {\n"
        "    .container { display:flex; flex-direction:row; }\n"
        "    .item-a { flex-shrink:1; }\n"
        "    .item-b { flex-shrink:2; }\n"
        "}\n"
        "panel:container(class=\"container\") {\n"
        "    panel:itemA(class=\"item-a\")\n"
        "    panel:itemB(class=\"item-b\")\n"
        "}\n";

    auto* root = env.load(ldt);
    EXPECT_NOT_NULL(root);

    auto* itemA = env.findById("itemA");
    auto* itemB = env.findById("itemB");
    EXPECT_NOT_NULL(itemA);
    EXPECT_NOT_NULL(itemB);

    // Both should be narrower than their original 200
    float aW = itemA->layout.getBorderBox().width;
    float bW = itemB->layout.getBorderBox().width;
    EXPECT_LT(aW, 200.0f);
    EXPECT_LT(bW, 200.0f);

    // B has higher shrink factor → narrower than A
    EXPECT_LT(bW, aW);
}
