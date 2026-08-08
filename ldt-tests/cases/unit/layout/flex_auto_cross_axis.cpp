#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"

// Regression: flex column 容器在主轴（高度）方向对子项触发 grow/shrink 后，
// 会调用 reMeasureChildren 重推子项的交叉轴（宽度）。当子项是 flex row 且宽度为
// auto 时，旧实现把重测可用宽度设为 UNBOUNDED(inf)，导致它被内容撑大/压缩，
// 而不是保持父容器（100% = 800）提供的可用宽度。
//
// 旧根因（box_model_engine.cpp reMeasureChildren）：
//   layoutFlex(col) 对 row（flex-grow:1, flex-shrink:1, 宽度 auto）调整主轴高度后，
//   调用 reMeasureChildren(row, widthDefinite=false, heightDefinite=true)。旧实现里
//   widthDefinite=false 且 row 宽度 auto → availW = UNBOUNDED(inf)，row 重新测量时
//   contentW = maxLineMain = 内容宽度（item 900），于是 row 被内容撑到 900，
//   而不是父级给的 800（外层 col 的 stretch 也因此无法把它拉回，因为 900 > 800）。
//
// 修复：flex 容器的 auto 交叉轴尺寸由父级 flex 布局决定，reMeasureChildren 重测时
// 用当前 content 尺寸作可用空间，而非 UNBOUNDED。修复后 row 保持父可用宽度 800。
TEST_CASE("layout/regression_auto_cross_axis_not_resized_by_content") {
    TestEnv env;
    std::string ldt =
        "@style {\n"
        "  .col { width:100%; height:100%; }\n"
        "  .item { width:900; height:700; }\n"
        "}\n"
        "@layout {\n"
        "  .col { display:flex; flex-direction:column; }\n"
        "  .row { display:flex; flex-direction:row; flex-grow:1; flex-shrink:1; }\n"
        "}\n"
        "panel:col(class=\"col\") {\n"
        "  panel:row(class=\"row\") {\n"
        "    panel:item(class=\"item\")\n"
        "  }\n"
        "}\n";

    env.load(ldt, 800.0f, 600.0f);

    auto* col = env.findById("col");
    auto* row = env.findById("row");
    auto* item = env.findById("item");
    EXPECT_NOT_NULL(col);
    EXPECT_NOT_NULL(row);
    EXPECT_NOT_NULL(item);

    // col 占满视口
    EXPECT_FLOAT_EQ(col->layout.computedWidth, 800.0f, 0.5f);
    // row 的 auto 宽度应保持父可用宽度 800（不被内容 900 撑大）
    EXPECT_FLOAT_EQ(row->layout.computedWidth, 800.0f, 0.5f);
    // item 保持自己的固定宽度
    EXPECT_FLOAT_EQ(item->layout.computedWidth, 900.0f, 0.5f);
}
