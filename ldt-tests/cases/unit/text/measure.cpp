#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

TEST_CASE("text/measure") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .mytext { font-size:14; }\n"
        "}\n"
        "text:myText(class=\"mytext\", value=\"Hello World\")\n";

    auto* root = env.load(ldt);
    EXPECT_NOT_NULL(root);

    auto* text = env.findById("myText");
    EXPECT_NOT_NULL(text);

    // Text should have a non-zero content width
    float w = text->layout.getContentBox().width;
    EXPECT_GT(w, 0.0f);

    // Height should be > 0 (at least one line)
    float h = text->layout.getContentBox().height;
    EXPECT_GT(h, 0.0f);
}
