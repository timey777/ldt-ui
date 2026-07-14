#include "android_dsl_scene.h"

#include <android/log.h>

#include <chrono>
#include <ctime>
#include <exception>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

#include "components/control_factory.h"
#include "components/control_manager.h"
#include "components/resolved_mount_service.h"
#include "components/text.h"
#include "engine/core/parse.h"
#include "engine/ui_event_system.h"
#include "misc/logger.h"

namespace {

constexpr const char* kLogTag = "LDTAndroidScene";
constexpr size_t kMaxDebugLogLines = 14;
constexpr const char* kPageShellSlotId = "pageSlot";
constexpr const char* kRouteValueId = "routeValue";
constexpr const char* kArchitectureValueId = "architectureValue";
constexpr const char* kStatusValueId = "statusValue";

AndroidDslScene* g_activeDebugScene = nullptr;

struct PageDescriptor {
    AndroidDemoPage page;
    const char* route;
    const char* title;
    const char* rootId;
    const char* navButtonId;
    const char* assetPath;
};

const PageDescriptor& getPageDescriptor(AndroidDemoPage page) {
    static const PageDescriptor home {
        AndroidDemoPage::Home,
        "/home",
        "Android Home",
        "androidPageHomeRoot",
        "btnNavHome",
        "pages/home.ldt",
    };
    static const PageDescriptor product {
        AndroidDemoPage::Product,
        "/product",
        "Android Product",
        "androidPageProductRoot",
        "btnNavProduct",
        "pages/product.ldt",
    };
    static const PageDescriptor summary {
        AndroidDemoPage::Summary,
        "/summary",
        "Android Summary",
        "androidPageSummaryRoot",
        "btnNavSummary",
        "pages/summary.ldt",
    };

    if (page == AndroidDemoPage::Product) {
        return product;
    }

    if (page == AndroidDemoPage::Summary) {
        return summary;
    }

    return home;
}

std::string joinLogLines(const std::vector<std::string>& lines) {
    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            oss << '\n';
        }
        oss << lines[i];
    }
    return oss.str();
}

std::string currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime {};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << millis.count();
    return oss.str();
}

void setTextValue(ldt::Scene* scene, const std::string& id, const std::string& value) {
    if (!scene || !scene->getControlManager()) {
        return;
    }

    auto control = scene->getControlManager()->findControlById(id);
    auto textControl = std::dynamic_pointer_cast<ldt::Text>(control);
    if (!textControl) {
        return;
    }

    textControl->setText(value);
}

void setActiveNavButton(ldt::Scene* scene, AndroidDemoPage page) {
    if (!scene || !scene->getControlManager()) {
        return;
    }

    static const AndroidDemoPage pages[] = {
        AndroidDemoPage::Home,
        AndroidDemoPage::Product,
        AndroidDemoPage::Summary,
    };

    for (auto candidate : pages) {
        auto control = scene->getControlManager()->findControlById(getPageDescriptor(candidate).navButtonId);
        if (!control) {
            continue;
        }

        if (candidate == page) {
            control->addClass("nav-active");
        }
        else {
            control->removeClass("nav-active");
        }
    }
}

class AssetPageComponent : public ldt::UIComponent {
public:
    explicit AssetPageComponent(std::string assetPath)
        : assetPath_(std::move(assetPath)) {
    }

    void onAttach(ldt::Scene* scene, const std::string& parentId) override {
        if (!scene || !scene->getControlManager()) {
            return;
        }

        const std::string snippetSource = loadAndroidAssetText(assetPath_);
        if (snippetSource.empty()) {
            LDT_ERROR("Android page asset load failed: " << assetPath_);
            return;
        }

        ldt::LDTParser parser;
        ldt::AstSnippet snippet;
        try {
            snippet = parser.parse(snippetSource);
        }
        catch (const std::exception& ex) {
            LDT_ERROR("Android page component parse error: " << ex.what());
            return;
        }

        auto mounted = ldt::ResolvedMountService::MountSnippetUnderId(
            scene->getResolvedTree(),
            parentId,
            snippet,
            scene->getDocumentRuntime(),
            ldt::ResolvedRuleScope::Local);
        if (!mounted) {
            LDT_ERROR("Android page component mount failed under " << parentId);
        }
    }

private:
    std::string assetPath_;
};

std::unique_ptr<ldt::UIComponent> createPageComponent(AndroidDemoPage page) {
    return std::make_unique<AssetPageComponent>(getPageDescriptor(page).assetPath);
}

}  // namespace

AndroidDslScene::~AndroidDslScene() {
    if (g_activeDebugScene == this) {
        g_activeDebugScene = nullptr;
    }
}

void appendAndroidDebugLog(const std::string& message) {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "%s", message.c_str());
    if (g_activeDebugScene) {
        g_activeDebugScene->appendDebugLog(message);
    }
}

void clearAndroidDebugLog() {
    if (g_activeDebugScene) {
        g_activeDebugScene->clearDebugLog();
    }
}

bool AndroidDslScene::setup() {
    g_activeDebugScene = this;
    clearDebugLog();

    if (!shellEventsBound_) {
        bindShellEvents();
        shellEventsBound_ = true;
    }

    bool ok = reload();
    if (ok) {
        appendDebugLog("android component router ready");
    }
    return ok;
}

void AndroidDslScene::setSource(std::string source) {
    source_ = std::move(source);
}

bool AndroidDslScene::reload() {
    return loadShellSource(source_);
}

bool AndroidDslScene::loadShellSource(const std::string& source) {
    if (!getDocumentRuntime()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "DocumentRuntime is not ready");
        return false;
    }

    if (source.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "DSL source is empty. No fallback available.");
        return false;
    }

    try {
        ldt::LDTParser parser;
        auto snippet = parser.parse(source);
        auto ast = snippet.root();
        if (!ast) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Parser returned empty AST");
            return false;
        }

        activePageComponent_.reset();
        activePageRootUid_.clear();
        getDocumentRuntime()->notifyASTUpdated(ast);
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "DSL parsed and submitted");
        refreshDebugLogView();
        navigateTo(currentPage_, false);
        return true;
    } catch (const std::exception& ex) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "DSL parse failed: %s", ex.what());
    } catch (...) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "DSL parse failed with unknown error");
    }

    return false;
}

void AndroidDslScene::appendDebugLog(const std::string& message) {
    if (message.empty()) {
        return;
    }

    std::ostringstream oss;
    oss << nextLogSequence_++ << ". [" << currentTimestamp() << "] " << message;
    debugLogLines_.push_back(oss.str());
    if (debugLogLines_.size() > kMaxDebugLogLines) {
        debugLogLines_.erase(debugLogLines_.begin(), debugLogLines_.begin() + 1);
    }
    refreshDebugLogView();
}

void AndroidDslScene::clearDebugLog() {
    debugLogLines_.clear();
    nextLogSequence_ = 1;
    refreshDebugLogView();
}

void AndroidDslScene::copyDebugLogToClipboard() {
    setClipboardString(joinLogLines(debugLogLines_));
    appendDebugLog("ui click target=btnCopyLogs copied=" + std::to_string(debugLogLines_.size()) + " lines");
}

void AndroidDslScene::bindShellEvents() {
    onClick("#btnFullscreen", [this](const ldt::UIEventSystem::UIEvent&) {
        appendDebugLog("ui click target=btnFullscreen");
        dispatchToggleFullscreen();
    });

    onClick("#btnCopyLogs", [this](const ldt::UIEventSystem::UIEvent&) {
        copyDebugLogToClipboard();
    });

    onClick("#btnNavHome", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Home);
    });
    onClick("#btnNavProduct", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Product);
    });
    onClick("#btnNavSummary", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Summary);
    });

    onClick("#btnHomeToProduct", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Product);
    });
    onClick("#btnHomeToSummary", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Summary);
    });
    onClick("#btnProductToHome", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Home);
    });
    onClick("#btnProductToSummary", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Summary);
    });
    onClick("#btnSummaryToHome", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Home);
    });
    onClick("#btnSummaryToProduct", [this](const ldt::UIEventSystem::UIEvent&) {
        navigateTo(AndroidDemoPage::Product);
    });

    getUIEventSystem()->bind(
        "#btnFullscreen",
        ldt::UIEventSystem::UIEventType::MouseDown,
        [this](const ldt::UIEventSystem::UIEvent&) {
            appendDebugLog("ui mouse-down target=btnFullscreen");
        });

    getUIEventSystem()->bind(
        "#btnFullscreen",
        ldt::UIEventSystem::UIEventType::MouseUp,
        [this](const ldt::UIEventSystem::UIEvent&) {
            appendDebugLog("ui mouse-up target=btnFullscreen");
        });
}

void AndroidDslScene::navigateTo(AndroidDemoPage nextPage, bool logNavigation) {
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

    if (logNavigation) {
        appendDebugLog(std::string("route change -> ") + getPageDescriptor(nextPage).route);
    }
}

void AndroidDslScene::clearActivePage() {
    if (activePageComponent_) {
        activePageComponent_->onDetach(this);
        activePageComponent_.reset();
    }

    if (activePageRootUid_.empty()) {
        return;
    }

    auto* factory = ldt::ControlFactory::getInstance();
    if (!factory) {
        activePageRootUid_.clear();
        return;
    }

    factory->RemoveResolvedNodeByUid(activePageRootUid_, getDocumentRuntime());
    factory->executePendingRemovals();
    activePageRootUid_.clear();
}

void AndroidDslScene::updateShellState() {
    const auto& page = getPageDescriptor(currentPage_);
    setTextValue(this, kRouteValueId, page.route);
    setTextValue(this, kArchitectureValueId, "Stable Scene + Swappable Android Page Component");
    setTextValue(this, kStatusValueId, std::string(page.title) + " mounted under #pageSlot");
    setActiveNavButton(this, currentPage_);
}

void AndroidDslScene::refreshDebugLogView() {
    auto* controlManager = getControlManager();
    if (!controlManager) {
        return;
    }

    auto control = controlManager->findControlById("debugLogView");
    auto textControl = std::dynamic_pointer_cast<ldt::Text>(control);
    if (!textControl) {
        return;
    }

    const std::string content = debugLogLines_.empty()
        ? std::string("waiting for touch logs...")
        : joinLogLines(debugLogLines_);
    textControl->setText(content);
}
