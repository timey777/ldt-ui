#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

// align-items: stretch 的交叉轴链式传播规则：
// 当整条链上的 flex 容器交叉轴都是 auto（未写死 height/width）时，
// 最内层内容尺寸决定每一级父容器的交叉尺寸；
// 直到遇到一个固定交叉尺寸的父容器，传播才停止——
// 该固定父容器之内的 auto 子容器会被 stretch 填满到固定尺寸，
// 而显式固定尺寸的最内层内容不会被 stretch 改变。

// 情况 1：整条链交叉轴全部 auto → 内容(inner 80)决定所有父级高度(80/80/80)
TEST_CASE("layout/align_stretch_auto_chain_grows_with_content") {
    TestEnv env;
    std::string source =
        "@style {"
        " .outer { width:400; }"
        " .mid { width:300; }"
        " .inner { width:100; height:80; }"
        "}"
        "@layout {"
        " .outer { display:flex; flex-direction:row; align-items:stretch; }"
        " .mid { display:flex; flex-direction:row; align-items:stretch; }"
        "}"
        "panel:outer(class=\"outer\") {"
        " panel:mid(class=\"mid\") {"
        "  panel:inner(class=\"inner\")"
        " }"
        "}";

    env.load(source);
    auto* outer = env.findById("outer");
    auto* mid = env.findById("mid");
    auto* inner = env.findById("inner");
    EXPECT_NOT_NULL(outer);
    EXPECT_NOT_NULL(mid);
    EXPECT_NOT_NULL(inner);

    // 无固定父级：内容尺寸沿交叉轴向上传播，所有 auto 父级都等于内容高度
    EXPECT_FLOAT_EQ(inner->layout.computedHeight, 80.0f, 0.5f);
    EXPECT_FLOAT_EQ(mid->layout.computedHeight, 80.0f, 0.5f);
    EXPECT_FLOAT_EQ(outer->layout.computedHeight, 80.0f, 0.5f);
}

// 情况 2：最外层固定(200) → 内容传播在此停止，中间的 auto 父级被 stretch 填满到固定尺寸
TEST_CASE("layout/align_stretch_fixed_outer_fills_intermediate") {
    TestEnv env;
    std::string source =
        "@style {"
        " .outer { width:400; height:200; }"
        " .mid { width:300; }"
        " .inner { width:100; height:80; }"
        "}"
        "@layout {"
        " .outer { display:flex; flex-direction:row; align-items:stretch; }"
        " .mid { display:flex; flex-direction:row; align-items:stretch; }"
        "}"
        "panel:outer(class=\"outer\") {"
        " panel:mid(class=\"mid\") {"
        "  panel:inner(class=\"inner\")"
        " }"
        "}";

    env.load(source);
    auto* outer = env.findById("outer");
    auto* mid = env.findById("mid");
    auto* inner = env.findById("inner");
    EXPECT_NOT_NULL(outer);
    EXPECT_NOT_NULL(mid);
    EXPECT_NOT_NULL(inner);

    // 固定父级保持自身尺寸
    EXPECT_FLOAT_EQ(outer->layout.computedHeight, 200.0f, 0.5f);
    // 固定父级之内的 auto 父级被 stretch 拉满到 200（而不是内容 80）
    EXPECT_FLOAT_EQ(mid->layout.computedHeight, 200.0f, 0.5f);
    // 最内层显式固定尺寸的内容保持 80，不被 stretch 改变
    EXPECT_FLOAT_EQ(inner->layout.computedHeight, 80.0f, 0.5f);
}

// 情况 3：固定父级在链中间(150) → 内容传播在它这里停止，其外层 auto 父级由固定父级决定
TEST_CASE("layout/align_stretch_fixed_mid_stops_content_propagation") {
    TestEnv env;
    std::string source =
        "@style {"
        " .outer { width:400; }"
        " .mid { width:300; height:150; }"
        " .inner { width:100; height:80; }"
        "}"
        "@layout {"
        " .outer { display:flex; flex-direction:row; align-items:stretch; }"
        " .mid { display:flex; flex-direction:row; align-items:stretch; }"
        "}"
        "panel:outer(class=\"outer\") {"
        " panel:mid(class=\"mid\") {"
        "  panel:inner(class=\"inner\")"
        " }"
        "}";

    env.load(source);
    auto* outer = env.findById("outer");
    auto* mid = env.findById("mid");
    auto* inner = env.findById("inner");
    EXPECT_NOT_NULL(outer);
    EXPECT_NOT_NULL(mid);
    EXPECT_NOT_NULL(inner);

    // 内容 80 传播到固定父级 mid 即停止
    EXPECT_FLOAT_EQ(inner->layout.computedHeight, 80.0f, 0.5f);
    EXPECT_FLOAT_EQ(mid->layout.computedHeight, 150.0f, 0.5f);
    // 固定父级之上的 auto 父级由固定父级(150)决定，而不是内容(80)
    EXPECT_FLOAT_EQ(outer->layout.computedHeight, 150.0f, 0.5f);
}
