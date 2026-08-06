#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

TEST_CASE("layout/min_max_percentage_uses_definite_parent_size") {
    TestEnv env;
    std::string source =
        "@style {"
        " .parent { width:50%; height:50%; }"
        " .child { width:300; height:10; max-width:50%; min-height:25%; }"
        "}"
        "panel:parent(class=\"parent\") {"
        " panel:child(class=\"child\")"
        "}";

    env.load(source, 800.0f, 400.0f);
    auto* parent = env.findById("parent");
    auto* child = env.findById("child");
    EXPECT_NOT_NULL(parent);
    EXPECT_NOT_NULL(child);
    EXPECT_FLOAT_EQ(parent->layout.computedWidth, 400.0f, 0.5f);
    EXPECT_FLOAT_EQ(parent->layout.computedHeight, 200.0f, 0.5f);
    EXPECT_FLOAT_EQ(child->layout.computedWidth, 200.0f, 0.5f);
    EXPECT_FLOAT_EQ(child->layout.computedHeight, 50.0f, 0.5f);
}

TEST_CASE("layout/min_wins_when_greater_than_max") {
    TestEnv env;
    std::string source =
        "@style { .item { width:50; height:50; min-width:120; max-width:80; } }"
        "panel:item(class=\"item\")";

    env.load(source);
    auto* item = env.findById("item");
    EXPECT_NOT_NULL(item);
    EXPECT_FLOAT_EQ(item->layout.computedWidth, 120.0f, 0.5f);
}

TEST_CASE("layout/width_and_height_use_border_box") {
    TestEnv env;
    std::string source =
        "@style { .item { width:100; height:80; padding:10; border:2px #000000; } }"
        "panel:item(class=\"item\")";

    env.load(source);
    auto* item = env.findById("item");
    EXPECT_NOT_NULL(item);
    EXPECT_FLOAT_EQ(item->layout.getBorderBox().width, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(item->layout.getBorderBox().height, 80.0f, 0.5f);
    EXPECT_FLOAT_EQ(item->layout.getContentBox().width, 76.0f, 0.5f);
    EXPECT_FLOAT_EQ(item->layout.getContentBox().height, 56.0f, 0.5f);
}
