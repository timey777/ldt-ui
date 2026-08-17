#include "test_case.h"
#include "test_env.h"
#include "test_assert.h"
#include "components/text.h"

// 复现 dashboard.ldt 的 header-row 场景：
//   text(class="th", width=300, text-align=center, value="Driver Name")
// 外加 @layout 里的 .th { display: block; }
static std::string makeHeaderLdt(bool thBlock) {
    std::string layoutRule = thBlock ? "    .th { display: block; }\n" : "";
    return
        "@style {\n"
        "    .th { padding: 14px 24px; font-size: 10px; font-weight: bold; }\n"
        "}\n"
        "@layout {\n"
        "    .header-row { display: grid; grid-template-columns: 300px 290px 250px 1fr; }\n"
        + layoutRule +
        "}\n"
        "panel:root {\n"
        "    panel(class=\"header-row\") {\n"
        "        text:th1(class=\"th\", width=300, text-align=center, value=\"Driver Name\"),\n"
        "        text:th2(class=\"th\", width=290, text-align=center, value=\"Current Route Status\")\n"
        "    }\n"
        "}\n";
}

TEST_CASE("text/grid_th_centering_debug") {
    // ── 有 .th { display: block; }：理论上 wrap=true，center 生效 ──
    {
        TestEnv env;
        auto* root = env.load(makeHeaderLdt(true));
        EXPECT_NOT_NULL(root);

        auto* th1 = env.findById("th1");
        EXPECT_NOT_NULL(th1);

        // 1) text-align=center 内联属性是否进入 finalStyle
        EXPECT_TRUE(th1->finalStyle.textAlign == ldt::TextAlign::Center);

        // 2) display:block => layout.wrap 应该为 true
        EXPECT_TRUE(th1->layout.wrap);

        // 3) width=300 是否生效（grid 列宽 300）
        EXPECT_FLOAT_EQ(th1->layout.getBorderBox().width, 300.0f, 1.0f);

        // 4) 控件实例上的状态（走 ControlFactory::makeControl）
        auto* scene = env.loadFull(makeHeaderLdt(true));
        EXPECT_NOT_NULL(scene);
        auto* th1b = env.findById("th1");
        EXPECT_NOT_NULL(th1b);
        auto ctrl = th1b->getControl().lock();
        EXPECT_NOT_NULL(ctrl.get());
        if (ctrl) {
            auto textCtrl = std::dynamic_pointer_cast<ldt::Text>(ctrl);
            EXPECT_NOT_NULL(textCtrl.get());
            if (textCtrl) {
                // Text::Alignment 枚举: Left=0, Center=1, Right=2
                EXPECT_TRUE(textCtrl->getAlignment() == ldt::Text::Alignment::Center);
                EXPECT_TRUE(textCtrl->getWrap());
            }
        }
    }

    // ── 没有 .th { display: block; }：基线，wrap 应为 false ──
    {
        TestEnv env;
        auto* root = env.load(makeHeaderLdt(false));
        EXPECT_NOT_NULL(root);
        auto* th1 = env.findById("th1");
        EXPECT_NOT_NULL(th1);
        EXPECT_TRUE(th1->finalStyle.textAlign == ldt::TextAlign::Center);
        EXPECT_TRUE(!th1->layout.wrap);
    }
}
