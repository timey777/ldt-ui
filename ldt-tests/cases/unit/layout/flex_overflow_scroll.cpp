#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

#include <algorithm>
#include <limits>

namespace {

std::string makeOverflowLayout(const char* panelSize) {
    std::string source =
        "@style {"
        " .title { font-size:18px; }"
        " .btn { width:100px; height:150px; padding:8px; }"
        " .v-box { height:50%; }"
        " .pnl { width:100px;" + std::string(panelSize) + " }"
        "}"
        "@layout {"
        " .v-box { display:flex; flex-direction:column; align-items:center; justify-content:center; }"
        " .title { flex-shrink:0; }"
        " .btn { flex-shrink:0; }"
        " .pnl { flex-grow:1; flex-shrink:1; }"
        "}"
        "panel:vbox(class=\"v-box\") {"
        " text:title(class=\"title\", value=\"Hello LDT\"),"
        " button:button(class=\"btn\") { text(value=\"Click Me\") }"
        " panel:pnl(class=\"pnl\") {";
    for (int i = 0; i < 21; ++i) {
        source += "text(class=\"title\", value=\"Hello LDT\"),";
    }
    source += "}}";
    return source;
}

void collectMaxTextBottom(const ldt::DisplayList& list, float& maxBottom) {
    for (const auto& command : list.getCommands()) {
        if (command.type == ldt::DrawCommandType::Text ||
            command.type == ldt::DrawCommandType::TextLayout) {
            maxBottom = std::max(maxBottom, command.bounds.bottom());
        }
        if (command.type == ldt::DrawCommandType::SubList && command.layer) {
            collectMaxTextBottom(*command.layer, maxBottom);
        }
    }
}

} // namespace

TEST_CASE("layout/flex_shrink_uses_remaining_height") {
    TestEnv env;
    env.load(makeOverflowLayout(""), 800.0f, 600.0f);

    auto* vbox = env.findById("vbox");
    auto* panel = env.findById("pnl");
    EXPECT_NOT_NULL(vbox);
    EXPECT_NOT_NULL(panel);

    EXPECT_FLOAT_EQ(panel->layout.getBorderBox().bottom(),
                    vbox->layout.getAbsoluteContentBox().bottom(), 0.5f);
    EXPECT_GT(panel->layout.scroll.scrollHeight, panel->layout.viewportHeight);
    EXPECT_FLOAT_EQ(vbox->layout.scroll.scrollHeight, vbox->layout.viewportHeight, 0.5f);
}

TEST_CASE("layout/flex_min_max_redistributes_space") {
    TestEnv env;
    std::string source =
        "@style {"
        " .container { width:400; height:100; }"
        " .item { width:50; height:50; }"
        " .limited { max-width:100; }"
        "}"
        "@layout {"
        " .container { display:flex; flex-direction:row; }"
        " .item { flex-grow:1; }"
        "}"
        "panel:container(class=\"container\") {"
        " panel:first(class=\"item limited\")"
        " panel:second(class=\"item\")"
        "}";
    env.load(source);

    auto* first = env.findById("first");
    auto* second = env.findById("second");
    EXPECT_NOT_NULL(first);
    EXPECT_NOT_NULL(second);
    EXPECT_FLOAT_EQ(first->layout.computedWidth, 100.0f, 0.5f);
    EXPECT_FLOAT_EQ(second->layout.computedWidth, 300.0f, 0.5f);
}

TEST_CASE("render/scroll_cache_tracks_content_offset") {
    TestEnv env;
    env.loadFull(makeOverflowLayout(" min-height:0;"), 800.0f, 600.0f);

    auto* panel = env.findById("pnl");
    EXPECT_NOT_NULL(panel);
    auto control = panel->getControl().lock();
    EXPECT_NOT_NULL(control.get());

    const float maxScrollY = panel->layout.scroll.scrollHeight - panel->layout.viewportHeight;
    EXPECT_GT(maxScrollY, 0.0f);
    control->setScroll(0.0f, maxScrollY);
    control->markLayerDirty();

    ldt::DisplayList displayList;
    control->render(displayList);

    float maxTextBottom = -std::numeric_limits<float>::infinity();
    collectMaxTextBottom(displayList, maxTextBottom);
    EXPECT_GT(maxTextBottom, panel->layout.getBorderBox().bottom());
}
