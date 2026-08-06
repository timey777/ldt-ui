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

    // Shrink factors are direct weights, so B takes twice A's reduction.
    float aW = itemA->layout.getBorderBox().width;
    float bW = itemB->layout.getBorderBox().width;
    EXPECT_FLOAT_EQ(aW, 166.67f, 0.5f);
    EXPECT_FLOAT_EQ(bW, 133.33f, 0.5f);
}

TEST_CASE("layout/flex_items_do_not_shrink_by_default") {
    TestEnv env;
    std::string source =
        "@style {"
        " .container { width:300; height:100; overflow:auto; }"
        " .item { width:200; height:50; }"
        "}"
        "@layout { .container { display:flex; flex-direction:row; } }"
        "panel:container(class=\"container\") {"
        " panel:itemA(class=\"item\")"
        " panel:itemB(class=\"item\")"
        "}";

    env.load(source);
    auto* container = env.findById("container");
    auto* itemA = env.findById("itemA");
    auto* itemB = env.findById("itemB");
    EXPECT_NOT_NULL(container);
    EXPECT_NOT_NULL(itemA);
    EXPECT_NOT_NULL(itemB);
    EXPECT_FLOAT_EQ(itemA->layout.computedWidth, 200.0f, 0.5f);
    EXPECT_FLOAT_EQ(itemB->layout.computedWidth, 200.0f, 0.5f);
    EXPECT_FLOAT_EQ(container->layout.scroll.scrollWidth, 400.0f, 0.5f);
}

TEST_CASE("layout/flex_fixed_chrome_preserves_size") {
    TestEnv env;
    std::string source =
        "@style {"
        " .app { width:800; height:600; overflow:hidden; }"
        " .toolbar { width:800; height:48; }"
        " .content { width:800; height:715; overflow:auto; }"
        " .status { width:800; height:28; }"
        "}"
        "@layout {"
        " .app { display:flex; flex-direction:column; }"
        " .content { flex-grow:1; flex-shrink:1; }"
        "}"
        "panel:app(class=\"app\") {"
        " panel:toolbar(class=\"toolbar\")"
        " panel:content(class=\"content\")"
        " panel:status(class=\"status\")"
        "}";

    env.load(source);
    auto* toolbar = env.findById("toolbar");
    auto* content = env.findById("content");
    auto* status = env.findById("status");
    EXPECT_NOT_NULL(toolbar);
    EXPECT_NOT_NULL(content);
    EXPECT_NOT_NULL(status);
    EXPECT_FLOAT_EQ(toolbar->layout.computedHeight, 48.0f, 0.5f);
    EXPECT_FLOAT_EQ(content->layout.computedHeight, 524.0f, 0.5f);
    EXPECT_FLOAT_EQ(status->layout.computedHeight, 28.0f, 0.5f);
}

TEST_CASE("layout/flex_shrink_redistributes_after_min_width") {
    TestEnv env;

    std::string ldt =
        "@style {"
        " .container { width:300; height:100; }"
        " .item { width:200; height:50; min-width:0; }"
        " .limited { min-width:180; }"
        "}"
        "@layout {"
        " .container { display:flex; flex-direction:row; }"
        " .item { flex-shrink:1; }"
        "}"
        "panel:container(class=\"container\") {"
        " panel:itemA(class=\"item limited\")"
        " panel:itemB(class=\"item\")"
        "}";

    env.load(ldt);
    auto* itemA = env.findById("itemA");
    auto* itemB = env.findById("itemB");
    EXPECT_NOT_NULL(itemA);
    EXPECT_NOT_NULL(itemB);
    EXPECT_FLOAT_EQ(itemA->layout.computedWidth, 180.0f, 0.5f);
    EXPECT_FLOAT_EQ(itemB->layout.computedWidth, 120.0f, 0.5f);
}
