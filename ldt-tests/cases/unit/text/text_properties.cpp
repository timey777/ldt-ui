#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

TEST_CASE("text/style_properties") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .demo { font-weight: bold; line-height: 30px; text-align: right; font-style: italic; }\n"
        "}\n"
        "text:myText(class=\"demo\", value=\"Hello World\")\n";

    auto* root = env.load(ldt);
    EXPECT_NOT_NULL(root);

    auto* text = env.findById("myText");
    EXPECT_NOT_NULL(text);

    EXPECT_TRUE(text->finalStyle.fontWeight == ldt::FontWeight::Bold);
    EXPECT_FLOAT_EQ(text->finalStyle.lineHeight, 30.0f, 0.05f);
    EXPECT_TRUE(text->finalStyle.textAlign == ldt::TextAlign::Right);
    EXPECT_TRUE(text->finalStyle.fontStyle == ldt::FontStyle::Italic);
}

TEST_CASE("text/line_height_and_alignment_are_applied") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .demo { line-height: 26px; text-align: center; }\n"
        "}\n"
        "text:myText(class=\"demo\", value=\"Hello\")\n";

    auto* root = env.load(ldt);
    EXPECT_NOT_NULL(root);

    auto* text = env.findById("myText");
    EXPECT_NOT_NULL(text);

    EXPECT_FLOAT_EQ(text->finalStyle.lineHeight, 26.0f, 0.05f);
    EXPECT_TRUE(text->finalStyle.textAlign == ldt::TextAlign::Center);
}
