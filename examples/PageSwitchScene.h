#pragma once

#include "components/ui_component.h"
#include "components/scene.h"

#include <memory>
#include <string>

namespace ldt {

enum class PageSwitchPageKind {
    Home,
    Product,
    Summary,
};

class PageSwitchScene : public Scene {
public:
    explicit PageSwitchScene(DocumentRuntime& runtime, PageSwitchPageKind pageKind = PageSwitchPageKind::Home)
        : Scene(runtime)
        , initialPage_(pageKind) {
    }

    ~PageSwitchScene() override = default;

    bool setup() override;

private:
    PageSwitchPageKind initialPage_;
    PageSwitchPageKind currentPage_ = PageSwitchPageKind::Home;
    std::unique_ptr<UIComponent> activePageComponent_;
    std::string activePageRootUid_;

    bool loadShellSource(const std::string& source);
    void bindShellEvents();
    void navigateTo(PageSwitchPageKind nextPage);
    void clearActivePage();
    void updateShellState();
    void returnToMainScene();
};

} // namespace ldt