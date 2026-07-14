#include "PageSwitchScene.h"

#include "MainScene.h"

#include "components/control_factory.h"
#include "components/control_manager.h"
#include "components/resolved_mount_service.h"
#include "components/stage.h"
#include "components/text.h"
#include "engine/core/parse.h"
#include "engine/ui_event_system.h"
#include "misc/logger.h"

#include <memory>
#include <string>
#include <utility>

namespace ldt {

namespace {

constexpr const char* kPageShellSlotId = "pageSlot";
constexpr const char* kRouteValueId = "routeValue";
constexpr const char* kArchitectureValueId = "architectureValue";
constexpr const char* kStatusValueId = "statusValue";

struct PageDescriptor {
    PageSwitchPageKind page;
    const char* route;
    const char* title;
    const char* rootId;
    const char* navButtonId;
};

const PageDescriptor& getPageDescriptor(PageSwitchPageKind pageKind)
{
    static const PageDescriptor home {
        PageSwitchPageKind::Home,
        "/home",
        "Home Dashboard",
        "pageHomeRoot",
        "btnNavHome",
    };
    static const PageDescriptor product {
        PageSwitchPageKind::Product,
        "/product",
        "Product Workspace",
        "pageProductRoot",
        "btnNavProduct",
    };
    static const PageDescriptor summary {
        PageSwitchPageKind::Summary,
        "/summary",
        "Summary Console",
        "pageSummaryRoot",
        "btnNavSummary",
    };

    if (pageKind == PageSwitchPageKind::Product) {
        return product;
    }

    if (pageKind == PageSwitchPageKind::Summary) {
        return summary;
    }

    return home;
}

void setTextValue(Scene* scene, const std::string& id, const std::string& value)
{
    if (!scene || !scene->getControlManager()) {
        return;
    }

    auto control = scene->getControlManager()->findControlById(id);
    auto text = std::dynamic_pointer_cast<Text>(control);
    if (!text) {
        return;
    }

    text->setText(value);
}

void setActiveNavButton(Scene* scene, PageSwitchPageKind pageKind)
{
    if (!scene || !scene->getControlManager()) {
        return;
    }

    static const PageSwitchPageKind allPages[] = {
        PageSwitchPageKind::Home,
        PageSwitchPageKind::Product,
        PageSwitchPageKind::Summary,
    };

    for (auto page : allPages) {
        auto control = scene->getControlManager()->findControlById(getPageDescriptor(page).navButtonId);
        if (!control) {
            continue;
        }

        if (page == pageKind) {
            control->addClass("nav-active");
        }
        else {
            control->removeClass("nav-active");
        }
    }
}

std::string buildShellSource()
{
    return R"ui(
@style {
    * {
        box-sizing: border-box;
    }
    .screen {
        width: 100%;
        height: 100%;
        padding: 24px;
        background-color: #e7edf4;
    }
    .shell {
        width: 100%;
        height: 100%;
        background-color: #f8fafc;
        border: 1px solid #d4dde8;
        border-radius: 20px;
        padding: 20px;
    }
    .shell-header {
        width: 100%;
        background-color: #ffffff;
        border: 1px solid #d4dde8;
        border-radius: 16px;
        padding: 18px;
        margin-bottom: 14px;
    }
    .eyebrow {
        font-size: 12px;
        margin-bottom: 6px;
    }
    .shell-title {
        font-size: 30px;
        margin-bottom: 10px;
    }
    .shell-copy {
        font-size: 15px;
        margin-bottom: 6px;
    }
    .shell-nav {
        width: 100%;
        margin-bottom: 14px;
    }
    .nav-btn {
        background-color: #dbe4ef;
        padding: 10px 14px;
        border-radius: 10px;
        margin-right: 10px;
    }
    .nav-active {
        background-color: #0f172a;
        color: #ffffff;
    }
    .back-btn {
        background-color: #475569;
        padding: 10px 14px;
        border-radius: 10px;
    }
    .status-strip {
        width: 100%;
        background-color: #ffffff;
        border: 1px solid #d4dde8;
        border-radius: 16px;
        padding: 14px;
        margin-bottom: 14px;
    }
    .status-label {
        font-size: 12px;
        margin-right: 8px;
    }
    .status-value {
        font-size: 14px;
        margin-right: 18px;
    }
    .page-slot {
        width: 100%;
        background-color: #ffffff;
        border: 1px solid #d4dde8;
        border-radius: 18px;
        padding: 18px;
    }
    .page-root {
        width: 100%;
    }
    .page-banner {
        width: 100%;
        border-radius: 16px;
        padding: 18px;
        margin-bottom: 14px;
    }
    .page-home {
        background-color: #eff6ff;
        border: 1px solid #bfdbfe;
    }
    .page-product {
        background-color: #ecfdf5;
        border: 1px solid #a7f3d0;
    }
    .page-summary {
        background-color: #fff7ed;
        border: 1px solid #fed7aa;
    }
    .page-title {
        font-size: 26px;
        margin-bottom: 8px;
    }
    .page-copy {
        font-size: 15px;
        margin-bottom: 6px;
    }
    .page-actions {
        margin-top: 10px;
        margin-bottom: 10px;
    }
    .page-btn-primary {
        background-color: #2563eb;
        padding: 10px 14px;
        border-radius: 10px;
        margin-right: 10px;
    }
    .page-btn-secondary {
        background-color: #0f172a;
        padding: 10px 14px;
        border-radius: 10px;
    }
    .card-grid {
        width: 100%;
    }
    .metric-card {
        width: 100%;
        background-color: #ffffff;
        border: 1px solid #d4dde8;
        border-radius: 14px;
        padding: 14px;
        margin-right: 12px;
    }
    .metric-label {
        font-size: 12px;
        margin-bottom: 6px;
    }
    .metric-value {
        font-size: 20px;
        margin-bottom: 4px;
    }
    .metric-copy {
        font-size: 13px;
    }
    .ops-list {
        width: 100%;
        background-color: #ffffff;
        border: 1px solid #d4dde8;
        border-radius: 14px;
        padding: 14px;
    }
    .ops-line {
        font-size: 14px;
        margin-bottom: 6px;
    }
}

@layout {
    .screen {
        display: flex;
        flex-direction: column;
    }
    .shell {
        display: flex;
        flex-direction: column;
    }
    .shell-header {
        display: flex;
        flex-direction: column;
    }
    .shell-nav {
        display: flex;
        flex-direction: row;
    }
    .status-strip {
        display: flex;
        flex-direction: row;
    }
    .page-slot {
        display: flex;
        flex-direction: column;
    }
    .page-root {
        display: flex;
        flex-direction: column;
    }
    .page-banner {
        display: flex;
        flex-direction: column;
    }
    .page-actions {
        display: flex;
        flex-direction: row;
    }
    .card-grid {
        display: flex;
        flex-direction: row;
    }
    .metric-card {
        display: flex;
        flex-direction: column;
    }
    .ops-list {
        display: flex;
        flex-direction: column;
    }
}

panel:root(class="screen") {
    panel:shell(class="shell") {
        panel:header(class="shell-header") {
            text:eyebrow(class="eyebrow", value="Component-driven page host")
            text:title(class="shell-title", value="Production-style Page Switching")
            text:copy1(class="shell-copy", value="The Scene stays mounted while the page content under #pageSlot is swapped as a UIComponent.")
            text:copy2(class="shell-copy", value="This keeps shell state, event registration, and host bindings stable.")
        }

        panel:navigation(class="shell-nav") {
            button:navHome(id="btnNavHome", class="nav-btn nav-active") {
                text(value="Home")
            }
            button:navProduct(id="btnNavProduct", class="nav-btn") {
                text(value="Product")
            }
            button:navSummary(id="btnNavSummary", class="nav-btn") {
                text(value="Summary")
            }
            button:backMain(id="btnBackMain", class="back-btn") {
                text(value="Back To Original Scene", color="#ffffff")
            }
        }

        panel:status(class="status-strip") {
            text(class="status-label", value="Route")
            text:routeValue(id="routeValue", class="status-value", value="/home")
            text(class="status-label", value="Architecture")
            text:architectureValue(id="architectureValue", class="status-value", value="Stable Scene + Page Components")
            text(class="status-label", value="Mount State")
            text:statusValue(id="statusValue", class="status-value", value="Home component mounted")
        }

        panel:pageSlot(id="pageSlot", class="page-slot") {}
    }
}
)ui";
}

class SnippetPageComponent : public UIComponent {
public:
    explicit SnippetPageComponent(std::string snippet)
        : snippet_(std::move(snippet)) {
    }

    void onAttach(Scene* scene, const std::string& parentId) override {
        if (!scene || !scene->getControlManager()) {
            return;
        }

        LDTParser parser;
        AstSnippet snippet;
        try {
            snippet = parser.parse(snippet_);
        }
        catch (const std::exception& ex) {
            LDT_ERROR("Page component parse error: " << ex.what());
            return;
        }

        auto mounted = ResolvedMountService::MountSnippetUnderId(
            scene->getResolvedTree(),
            parentId,
            snippet,
            scene->getDocumentRuntime(),
            ResolvedRuleScope::Local);

        if (!mounted) {
            LDT_ERROR("Page component mount failed under " << parentId);
        }
    }

private:
    std::string snippet_;
};

std::string buildHomePageComponentSource()
{
    return R"ui(
panel:pageHomeRoot(class="page-root") {
    panel:hero(class="page-banner page-home") {
        text:title(class="page-title", value="Home dashboard keeps the shell alive")
        text:copy1(class="page-copy", value="This page is mounted as a component under the slot container instead of replacing the whole Scene.")
        text:copy2(class="page-copy", value="Shared chrome, route state, and back navigation stay centralized in the shell.")
        panel:actions(class="page-actions") {
            button:primary(id="btnHomeToProduct", class="page-btn-primary") {
                text(value="Go To Product", color="#ffffff")
            }
            button:secondary(id="btnHomeToSummary", class="page-btn-secondary") {
                text(value="Go To Summary", color="#ffffff")
            }
        }
    }

    panel:grid(class="card-grid") {
        panel:card1(class="metric-card") {
            text(class="metric-label", value="Shell lifetime")
            text(class="metric-value", value="Stable")
            text(class="metric-copy", value="Header and nav do not rebuild on every route change.")
        }
        panel:card2(class="metric-card") {
            text(class="metric-label", value="Event binding")
            text(class="metric-value", value="One time")
            text(class="metric-copy", value="Scene-level bindings are registered once and reused across page components.")
        }
        panel:card3(class="metric-card") {
            text(class="metric-label", value="Swap target")
            text(class="metric-value", value="#pageSlot")
            text(class="metric-copy", value="Only the active page subtree is mounted and removed.")
        }
    }
}
)ui";
}

std::string buildProductPageComponentSource()
{
    return R"ui(
panel:pageProductRoot(class="page-root") {
    panel:hero(class="page-banner page-product") {
        text:title(class="page-title", value="Product workspace demonstrates deeper page content")
        text:copy1(class="page-copy", value="The page component can carry a richer subtree without changing the router contract.")
        text:copy2(class="page-copy", value="This is closer to a production pattern: Scene owns routing, components own page content.")
        panel:actions(class="page-actions") {
            button:primary(id="btnProductToHome", class="page-btn-primary") {
                text(value="Return Home", color="#ffffff")
            }
            button:secondary(id="btnProductToSummary", class="page-btn-secondary") {
                text(value="Go To Summary", color="#ffffff")
            }
        }
    }

    panel:ops(class="ops-list") {
        text(class="ops-line", value="Checklist: clear previous page by uid before mounting the next component.")
        text(class="ops-line", value="Checklist: keep nav selectors stable so bindings are registered once per Scene.")
        text(class="ops-line", value="Checklist: page root ids stay route-specific for deterministic mount tracking.")
    }
}
)ui";
}

std::string buildSummaryPageComponentSource()
{
    return R"ui(
panel:pageSummaryRoot(class="page-root") {
    panel:hero(class="page-banner page-summary") {
        text:title(class="page-title", value="Summary console closes the routing loop")
        text:copy1(class="page-copy", value="This page proves the shell can keep switching components without Scene replacement.")
        text:copy2(class="page-copy", value="The same host pattern can be adapted to any platform with a native shell.")
        panel:actions(class="page-actions") {
            button:primary(id="btnSummaryToHome", class="page-btn-primary") {
                text(value="Return Home", color="#ffffff")
            }
            button:secondary(id="btnSummaryToProduct", class="page-btn-secondary") {
                text(value="Back To Product", color="#ffffff")
            }
        }
    }

    panel:grid(class="card-grid") {
        panel:card1(class="metric-card") {
            text(class="metric-label", value="Routing")
            text(class="metric-value", value="Centralized")
            text(class="metric-copy", value="One Scene owns route changes and shell-wide affordances.")
        }
        panel:card2(class="metric-card") {
            text(class="metric-label", value="Pages")
            text(class="metric-value", value="Composable")
            text(class="metric-copy", value="Each page remains a UIComponent that can be expanded independently.")
        }
        panel:card3(class="metric-card") {
            text(class="metric-label", value="Next step")
            text(class="metric-value", value="Portable")
            text(class="metric-copy", value="Lift this host pattern into any platform and keep the scene shell native there too.")
        }
    }
}
)ui";
}

std::unique_ptr<UIComponent> createPageComponent(PageSwitchPageKind pageKind)
{
    if (pageKind == PageSwitchPageKind::Product) {
        return std::make_unique<SnippetPageComponent>(buildProductPageComponentSource());
    }

    if (pageKind == PageSwitchPageKind::Summary) {
        return std::make_unique<SnippetPageComponent>(buildSummaryPageComponentSource());
    }

    return std::make_unique<SnippetPageComponent>(buildHomePageComponentSource());
}

} // namespace

bool PageSwitchScene::setup()
{
    if (!loadShellSource(buildShellSource())) {
        return false;
    }

    bindShellEvents();
    navigateTo(initialPage_);
    return true;
}

bool PageSwitchScene::loadShellSource(const std::string& source)
{
    try {
        LDTParser parser;
        auto snippet = parser.parse(source);
        auto ast = snippet.root();
        if (!ast || !getDocumentRuntime()) {
            return false;
        }

        getDocumentRuntime()->notifyASTUpdated(ast);
        return true;
    }
    catch (const std::exception& ex) {
        LDT_ERROR("PageSwitchScene parse error: " << ex.what());
        return false;
    }
}

void PageSwitchScene::bindShellEvents()
{
    onClick("#btnNavHome", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Home);
    });
    onClick("#btnNavProduct", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Product);
    });
    onClick("#btnNavSummary", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Summary);
    });

    onClick("#btnHomeToProduct", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Product);
    });
    onClick("#btnHomeToSummary", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Summary);
    });
    onClick("#btnProductToHome", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Home);
    });
    onClick("#btnProductToSummary", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Summary);
    });
    onClick("#btnSummaryToHome", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Home);
    });
    onClick("#btnSummaryToProduct", [this](const UIEventSystem::UIEvent&) {
        navigateTo(PageSwitchPageKind::Product);
    });

    onClick("#btnBackMain", [this](const UIEventSystem::UIEvent&) {
        returnToMainScene();
    });
}

void PageSwitchScene::navigateTo(PageSwitchPageKind nextPage)
{
    clearActivePage();

    activePageComponent_ = createPageComponent(nextPage);
    if (!activePageComponent_) {
        return;
    }

    activePageComponent_->onAttach(this, kPageShellSlotId);
    currentPage_ = nextPage;

    auto* controlManager = getControlManager();
    if (!controlManager) {
        return;
    }

    auto pageRoot = controlManager->findControlById(getPageDescriptor(nextPage).rootId);
    activePageRootUid_ = pageRoot ? pageRoot->getUid() : std::string();
    updateShellState();
}

void PageSwitchScene::clearActivePage()
{
    if (activePageComponent_) {
        activePageComponent_->onDetach(this);
        activePageComponent_.reset();
    }

    if (activePageRootUid_.empty()) {
        return;
    }

    auto* factory = ControlFactory::getInstance();
    if (!factory) {
        activePageRootUid_.clear();
        return;
    }

    factory->RemoveResolvedNodeByUid(activePageRootUid_, getDocumentRuntime());
    factory->executePendingRemovals();
    activePageRootUid_.clear();
}

void PageSwitchScene::updateShellState()
{
    const auto& page = getPageDescriptor(currentPage_);
    setTextValue(this, kRouteValueId, page.route);
    setTextValue(this, kArchitectureValueId, "Stable Scene + Swappable UIComponent Page");
    setTextValue(this, kStatusValueId, std::string(page.title) + " mounted under #pageSlot");
    setActiveNavButton(this, currentPage_);
}

void PageSwitchScene::returnToMainScene()
{
    clearActivePage();

    auto* runtime = getDocumentRuntime();
    if (!runtime) {
        return;
    }

    auto nextScene = std::make_shared<ExampleMainScene>(*runtime);
    Stage::getInstance().replaceScene(nextScene);
}

} // namespace ldt
