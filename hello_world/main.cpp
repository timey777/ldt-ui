#include <application.h>
#include <components/stage.h>
#include <components/scene.h>
#include <engine/core/parse.h>
using namespace ldt;

// ── 最小 Scene：加载 DSL 并生成界面 ──
class DemoScene : public Scene {
public:
    using Scene::Scene;

    bool setup() override {
        LDTParser parser;
        auto snippet = parser.parse(R"ui(
            @style {
                .title { font-size: 18px; color: #333333; }
                .btn { width: 100px; padding: 8px; background-color: #007bff; color: #ffffff; border-radius: 4px; }
            }
            @layout {
                .v-box { display: flex; flex-direction: column; align-items: center; justify-content: center; }
            }
            panel(class="v-box") {
                text(class="title", value="Hello LDT"),
                button(class="btn") { text(value="Click Me") }
            }
        )ui");
        getDocumentRuntime()->notifyASTUpdated(snippet.root());
        return true;
    }
};

// ── 继承 Application，在 build 时创建场景 ──
class MyApp : public Application {
public:
    MyApp(const std::string& title, int winW, int winH)
        : Application(title, static_cast<float>(winW), static_cast<float>(winH))
    {
    }

    bool build(DocumentRuntime& runtime, Compositor&, float, float) override {
        auto scene = std::make_shared<DemoScene>(runtime);
        Stage::getInstance().replaceScene(scene);
        return true;
    }
};

int main(int argc, char* argv[]) {
    MyApp app("Hello LDT", 800, 600);
    return app.run(argc, argv);
}
