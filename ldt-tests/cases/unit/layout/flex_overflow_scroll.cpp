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

// 交叉轴（高度）上 stretch 只拉伸不压缩：canvas-area 高度由内容决定（400 + 160×2 = 720），
// 不会被压缩回 main 的 500。因此内容 720 溢出 main(500)，main 应出现垂直滚动条。
TEST_CASE("layout/flex_overflow_stays_with_shrunk_child") {
    TestEnv env;
    std::string source =
        "@style {"
        " .main { width:800; height:500; overflow:auto; }"
        " .canvas-area { width:100%; padding:160; overflow:auto; }"
        " .canvas { width:600; height:400; }"
        " .sidebar { width:260; height:100%; }"
        "}"
        "@layout {"
        " .main { display:flex; flex-direction:row; }"
        " .canvas-area { display:flex; flex-grow:1; flex-shrink:1;"
        "                align-items:center; justify-content:center; }"
        "}"
        "panel:main(class=\"main\") {"
        " panel:canvasArea(class=\"canvas-area\") {"
        "  panel:canvas(class=\"canvas\")"
        " }"
        " panel:sidebar(class=\"sidebar\")"
        "}";

    env.load(source, 800.0f, 500.0f);
    auto* main = env.findById("main");
    auto* canvasArea = env.findById("canvasArea");
    auto* canvas = env.findById("canvas");
    EXPECT_NOT_NULL(main);
    EXPECT_NOT_NULL(canvasArea);
    EXPECT_NOT_NULL(canvas);

    EXPECT_FLOAT_EQ(canvasArea->layout.padding.left, 160.0f, 0.5f);
    EXPECT_FLOAT_EQ(canvasArea->layout.padding.right, 160.0f, 0.5f);
    // stretch 只拉伸不压缩：canvas-area 高度 = 内容 400 + padding 320 = 720
    EXPECT_FLOAT_EQ(canvasArea->layout.getBorderBox().height, 720.0f, 0.5f);
    EXPECT_FLOAT_EQ(canvas->layout.getBorderBox().x,
                    canvasArea->layout.getAbsoluteContentBox().x - 190.0f, 0.5f);
    // 渲染端以 scrollHeight > viewportHeight 决定是否显示滚动条（hasVBar/hasHBar 是未使用的字段）
    // 内容 720 溢出 main(500)：main 应有垂直滚动条
    EXPECT_GT(main->layout.scroll.scrollHeight, main->layout.viewportHeight);
    // canvas 600 宽 > canvas-area shrink 后的内容区（content 220）：canvas-area 应有水平滚动条
    EXPECT_GT(canvasArea->layout.scroll.scrollWidth, canvasArea->layout.viewportWidth);
}

TEST_CASE("layout/flex_visible_overflow_remains_parent_overflow") {
    TestEnv env;
    std::string source =
        "@style {"
        " .main { width:800; height:500; overflow:auto; }"
        " .content { overflow:visible; }"
        " .tall { width:100; height:700; }"
        "}"
        "@layout {"
        " .main { display:flex; flex-direction:row; }"
        "}"
        "panel:main(class=\"main\") {"
        " panel:content(class=\"content\") {"
        "  panel(class=\"tall\")"
        " }"
        "}";

    env.load(source, 800.0f, 500.0f);
    auto* main = env.findById("main");
    auto* content = env.findById("content");
    EXPECT_NOT_NULL(main);
    EXPECT_NOT_NULL(content);

    EXPECT_FLOAT_EQ(content->layout.getBorderBox().height, 700.0f, 0.5f);
    // 渲染端以 scrollHeight > viewportHeight 决定是否显示滚动条（hasVBar/hasHBar 是未使用的字段）
    // 内容 700 溢出 main(500)：main 应有垂直滚动条
    EXPECT_GT(main->layout.scroll.scrollHeight, main->layout.viewportHeight);
    // content 是 overflow:visible，自身不出现滚动条（溢出冒泡给 main）
    EXPECT_LE(content->layout.scroll.scrollHeight, content->layout.viewportHeight);
}

TEST_CASE("layout/flex_center_preserves_negative_free_space") {
    TestEnv env;
    std::string source =
        "@style {"
        " .row { width:300; height:80; }"
        " .item { width:100; height:80; }"
        "}"
        "@layout {"
        " .row { display:flex; justify-content:center; }"
        " .item { flex-shrink:0; }"
        "}"
        "panel:row(class=\"row\") {"
        " panel:first(class=\"item\"), panel(class=\"item\"),"
        " panel(class=\"item\"), panel(class=\"item\")"
        "}";

    env.load(source, 800.0f, 600.0f);
    auto* row = env.findById("row");
    auto* first = env.findById("first");
    EXPECT_NOT_NULL(row);
    EXPECT_NOT_NULL(first);

    EXPECT_FLOAT_EQ(first->layout.getBorderBox().x,
                    row->layout.getAbsoluteContentBox().x - 50.0f, 0.5f);
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
