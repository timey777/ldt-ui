#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"
#include "components/text.h"
#include "components/control_factory.h"

// 验证：Text 的字体属性（color/font-size/font-weight 等）在运行时样式变化后
// 是否会通过同步路径刷新到控件。
// 现状：这些属性只在 makeControl（创建时）设置一次，syncFromResolvedNode /
// OnSyncFromResolvedNode 不重新同步 → 预期此测试失败（证明 bug 存在）。
TEST_CASE("text/runtime_style_changes_refresh_font_props") {
    TestEnv env;

    std::string ldt =
        "@style {\n"
        "    .demo { color: #ff0000; font-size: 18px; font-weight: bold; }\n"
        "}\n"
        "text:myText(class=\"demo\", value=\"Hello\")\n";

    auto* scene = env.loadFull(ldt);
    EXPECT_NOT_NULL(scene);
    auto* text = env.findById("myText");
    EXPECT_NOT_NULL(text);
    auto ctrl = text->getControl().lock();
    EXPECT_NOT_NULL(ctrl.get());
    auto textCtrl = std::dynamic_pointer_cast<ldt::Text>(ctrl);
    EXPECT_NOT_NULL(textCtrl.get());
    if (!textCtrl) return;

    // ── 创建时的初始值（来自 .demo 样式）──
    EXPECT_TRUE(textCtrl->isBold());
    EXPECT_FLOAT_EQ(textCtrl->getFontSize(), 18.0f, 0.1f);
    EXPECT_FLOAT_EQ(textCtrl->getTextColor().r, 1.0f, 0.001f); // #ff0000
    EXPECT_FLOAT_EQ(textCtrl->getTextColor().g, 0.0f, 0.001f);
    EXPECT_FLOAT_EQ(textCtrl->getTextColor().b, 0.0f, 0.001f);

    // ── 模拟运行时样式变化（正常流程由 UIState 变化 → recomputeStyle 更新 finalStyle）──
    text->finalStyle.textColor = ldt::ui::Color(0.0f, 0.0f, 1.0f, 1.0f); // 红 → 蓝
    text->finalStyle.fontSize = 32.0f;
    text->finalStyle.fontWeight = ldt::FontWeight::Normal;

    // ── 更新路径：重新同步 ──
    ldt::ControlFactory::getInstance()->SyncPropertiesFromResolvedNode(text, ctrl);

    // ── 期望控件反映新样式；若失败 → 证明"更新时不刷新字体属性"的 bug ──
    EXPECT_FLOAT_EQ(textCtrl->getFontSize(), 32.0f, 0.1f);
    EXPECT_TRUE(!textCtrl->isBold());
    EXPECT_FLOAT_EQ(textCtrl->getTextColor().r, 0.0f, 0.001f);
    EXPECT_FLOAT_EQ(textCtrl->getTextColor().g, 0.0f, 0.001f);
    EXPECT_FLOAT_EQ(textCtrl->getTextColor().b, 1.0f, 0.001f);
}
