#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

#include <sstream>
#include <string>

// ============================================================================
// 验证 flex 列表"有余量拉伸、超出滚动、不越界"的写法。
//
//   结构：view(v-box, 100%x100%)
//           main(h-box, flex-grow:1, flex-shrink:1)
//             sidebar(v-box, width:220)      ← 由 main stretch 钉死高度
//               header(height:30)
//               list(session-list, flex-grow:1, flex-shrink:1)
//                 session × N (height:30)
//
// 引擎流程：measure(自底向上量内在) → resolve(自顶向下定最终尺寸) → position(定位)
// 交叉轴压缩在 resolve 阶段完成：单行 lineCross = 容器交叉尺寸，
// 子项 overflow != visible 时被 stretch 压回容器（CSS 语义）。
// ============================================================================
namespace {

std::string makeChainSource(const char* mainLayout, const char* listStyle, const char* listLayout, int sessionCount) {
    std::ostringstream src;
    src << "@style {"
        << " .view { width:100%; height:100%; }"
        << " .sidebar { width:220px; }"
        << " .header { height:30px; }"
        << " .list { width:100%; " << listStyle << " }"
        << " .session { width:100%; height:30px; }"
        << "}"
        << "@layout {"
        << " .view { display:flex; flex-direction:column; }"
        << " .main { display:flex; flex-direction:row; flex-grow:1; " << mainLayout << " }"
        << " .sidebar { display:flex; flex-direction:column; }"
        << " .header { flex-shrink:0; }"
        << " .list { flex-grow:1; " << listLayout << " }"
        << "}"
        << "panel:view(class=\"view\") {"
        << " panel:main(class=\"main\") {"
        << "  panel:sidebar(class=\"sidebar\") {"
        << "   panel:header(class=\"header\")"
        << "   panel:list(class=\"list\") {";
    for (int i = 0; i < sessionCount; ++i) {
        src << "panel(class=\"session\"),";
    }
    src << "   }"
        << "  }"
        << " }"
        << "}";
    return src.str();
}

} // namespace

// 可行方案：list = height:0 + flex-grow:1（CSS flex-basis:0 写法）。
// 内容多(30×30=900) → list 从 0 grow 到剩余空间 570 → 溢出出滚动条；main 不溢出 viewport。
TEST_CASE("layout/grow_shrink_chain_height_zero_scrolls") {
    TestEnv env;
    env.load(makeChainSource("flex-shrink:1;", "height:0px;", "flex-shrink:1;", 30), 800.0f, 600.0f);
    auto* main = env.findById("main");
    auto* list = env.findById("list");
    EXPECT_NOT_NULL(main);
    EXPECT_NOT_NULL(list);

    // main 被 view(600) 限住，不溢出
    EXPECT_LE(main->layout.getBorderBox().height, 600.5f);
    // list 被 grow 到 sidebar 剩余高度 570（而不是内容 900）
    EXPECT_FLOAT_EQ(list->layout.computedHeight, 570.0f, 0.5f);
    // 内容 900 > viewport 570 → 垂直滚动条
    EXPECT_GT(list->layout.scroll.scrollHeight, list->layout.viewportHeight);
}

// 内容少(5×30=150) → list 拉伸填满剩余空间 570，无滚动条。
TEST_CASE("layout/grow_shrink_chain_fills_when_spare") {
    TestEnv env;
    env.load(makeChainSource("flex-shrink:1;", "height:0px;", "flex-shrink:1;", 5), 800.0f, 600.0f);
    auto* main = env.findById("main");
    auto* list = env.findById("list");
    EXPECT_NOT_NULL(main);
    EXPECT_NOT_NULL(list);

    EXPECT_LE(main->layout.getBorderBox().height, 600.5f);
    EXPECT_FLOAT_EQ(list->layout.computedHeight, 570.0f, 0.5f);
    EXPECT_LE(list->layout.scroll.scrollHeight, list->layout.viewportHeight);
}

// 纯 flex-shrink:1 链（list 不写 height）：resolve 阶段交叉轴压缩后即可工作。
// main 收缩到 600 → sidebar 交叉轴被压回 600 → list 有受限容器 → 收缩到 570 → 滚动条。
TEST_CASE("layout/grow_shrink_chain_pure_shrink_scrolls") {
    TestEnv env;
    env.load(makeChainSource("flex-shrink:1;", "", "flex-shrink:1;", 30), 800.0f, 600.0f);
    auto* main = env.findById("main");
    auto* sidebar = env.findById("sidebar");
    auto* list = env.findById("list");
    EXPECT_NOT_NULL(main);
    EXPECT_NOT_NULL(sidebar);
    EXPECT_NOT_NULL(list);

    EXPECT_LE(main->layout.getBorderBox().height, 600.5f);
    EXPECT_FLOAT_EQ(sidebar->layout.computedHeight, 600.0f, 0.5f);
    EXPECT_FLOAT_EQ(list->layout.computedHeight, 570.0f, 0.5f);
    EXPECT_GT(list->layout.scroll.scrollHeight, list->layout.viewportHeight);
}

// 对照（默认行为文档化）：main 默认 flex-shrink:0 + list auto 高度。
// 内容超高时整条链被撑开，main 溢出 viewport，list 无滚动条 —— 这正是最初"撑高"的原因。
TEST_CASE("layout/grow_shrink_chain_shrink0_main_overflows") {
    TestEnv env;
    env.load(makeChainSource("", "", "", 30), 800.0f, 600.0f);
    auto* main = env.findById("main");
    auto* list = env.findById("list");
    EXPECT_NOT_NULL(main);
    EXPECT_NOT_NULL(list);

    EXPECT_GT(main->layout.getBorderBox().height, 600.0f);
    EXPECT_FLOAT_EQ(list->layout.computedHeight, 900.0f, 0.5f);
    EXPECT_LE(list->layout.scroll.scrollHeight, list->layout.viewportHeight);
}

// ============================================================
// 多个 session-list 均分场景（sidebar 里多个 flex-grow:1 子项）
// ============================================================
namespace {

std::string makeMultiListSource(const char* listStyle, const char* listLayout,
                                std::initializer_list<int> perListSessions) {
    std::ostringstream src;
    src << "@style {"
        << " .view { width:100%; height:100%; }"
        << " .sidebar { width:220px; }"
        << " .header { height:30px; }"
        << " .list { width:100%; " << listStyle << " }"
        << " .session { width:100%; height:30px; }"
        << "}"
        << "@layout {"
        << " .view { display:flex; flex-direction:column; }"
        << " .main { display:flex; flex-direction:row; flex-grow:1; flex-shrink:1; }"
        << " .sidebar { display:flex; flex-direction:column; }"
        << " .header { flex-shrink:0; }"
        << " .list { flex-grow:1; flex-shrink:1; " << listLayout << " }"
        << "}"
        << "panel:view(class=\"view\") {"
        << " panel:main(class=\"main\") {"
        << "  panel:sidebar(class=\"sidebar\") {"
        << "   panel:header(class=\"header\")";
    int i = 0;
    for (int n : perListSessions) {
        src << "   panel:list" << i << "(class=\"list\") {";
        for (int j = 0; j < n; ++j) src << "panel(class=\"session\"),";
        src << "}";
        ++i;
    }
    src << "  }"
        << " }"
        << "}";
    return src.str();
}

} // namespace

// 多个 list 用 auto 高度（flex-basis:auto）：子项内容高度参与"基准尺寸"，
// 内容多的 list 把空间吃掉，内容少的被压到 0 —— 非均分，且相互影响。
TEST_CASE("layout/multi_list_auto_basis_content_affects_height") {
    TestEnv env;
    env.load(makeMultiListSource("", "", {5, 30}), 800.0f, 600.0f);
    auto* a = env.findById("list0");
    auto* b = env.findById("list1");
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);

    // 可用 570；内容 150 vs 900 参与基准，shrink 后小 list 被压到 0，大 list 吃满 570
    EXPECT_FLOAT_EQ(a->layout.computedHeight, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(b->layout.computedHeight, 570.0f, 0.5f);
}

// 多个 list 用 height:0（flex-basis:0）：内容不参与基准尺寸，
// 严格均分 285/285，各自独立滚动（小的无滚动条，大的有滚动条）。
TEST_CASE("layout/multi_list_zero_basis_even_split") {
    TestEnv env;
    env.load(makeMultiListSource("height:0px;", "", {5, 30}), 800.0f, 600.0f);
    auto* a = env.findById("list0");
    auto* b = env.findById("list1");
    EXPECT_NOT_NULL(a);
    EXPECT_NOT_NULL(b);

    // 可用 570，两个 grow:1 → 均分 285/285，与各自内容无关
    EXPECT_FLOAT_EQ(a->layout.computedHeight, 285.0f, 0.5f);
    EXPECT_FLOAT_EQ(b->layout.computedHeight, 285.0f, 0.5f);
    // 内容不影响的证明：list0 只有 5 个(150) 无滚动条，list1 有 30 个(900) 出滚动条
    EXPECT_LE(a->layout.scroll.scrollHeight, a->layout.viewportHeight);
    EXPECT_GT(b->layout.scroll.scrollHeight, b->layout.viewportHeight);
}
