// MainScene.cpp
#include "MainScene.h"

#include "PageSwitchScene.h"

#include "components/stage.h"
#include "components/input.h"
#include "components/button.h"
#include "engine/ui_event_system.h"
#include "components/control_manager.h"
#include "components/text.h"
#include "engine/core/parse.h"
#include "misc/logger.h"

#include <chrono>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <ctime>
#include <iomanip>

#include "TestComponent.h"

namespace ldt {

namespace {

constexpr size_t kMaxDebugLogLines = 18;

std::string joinLogLines(const std::vector<std::string>& lines)
{
    std::ostringstream oss;
    for (size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            oss << '\n';
        }
        oss << lines[index];
    }
    return oss.str();
}

std::string currentTimestamp()
{
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

} // namespace

ExampleMainScene::ExampleMainScene(DocumentRuntime& runtime) : Scene(runtime) {}

ExampleMainScene::~ExampleMainScene()
{
    logSubscription_.reset();
}

bool ExampleMainScene::setup()
{
    logSubscription_.reset();
    logSubscription_ = getLogger().subscribe(*this);

    clearDebugLog();

    const std::string defaultSrc = R"ui(
@style {
    * {
        box-sizing: border-box;
    }
    .screen {
        width: 100%;
        height: 100%;
        background-color: #eef3f8;
    }
    .shell {
        width: 100%;
        height: 100%;
        background-color: #f8fafc;
        border: 1px solid #d9e2ec;
        border-radius: 18px;
        padding: 18px;
    }
    .shell-header {
        background-color: #ffffff;
        border: 1px solid #d9e2ec;
        border-radius: 16px;
        padding: 18px;
        margin-bottom: 14px;
    }
    .shell-nav {
        width: 100%;
        margin-bottom: 14px;
    }
    .nav-btn {
        background-color: #dbe4ef;
        color: #0f172a;
        padding: 10px 14px;
        border-radius: 10px;
        margin-right: 10px;
    }
    .nav-active {
        background-color: #0f172a;
        color: #ffffff;
    }
    .status-strip {
        width: 100%;
        background-color: #ffffff;
        border: 1px solid #d9e2ec;
        border-radius: 14px;
        padding: 14px;
        margin-bottom: 14px;
    }
    .status-label {
        font-size: 12px;
        margin-right: 8px;
    }
    .status-value {
        font-size: 14px;
        margin-right: 16px;
    }
    .page-slot {
        width: 100%;
        background-color: #ffffff;
        border: 1px solid #d9e2ec;
        border-radius: 16px;
        padding: 16px;
        margin-bottom: 14px;
    }
    .debug-panel {
        width: 100%;
        background-color: #0f172a;
        border: 1px solid #1e293b;
        border-radius: 14px;
        padding: 16px;
    }
    .title {
        font-size: 28px;
        margin-bottom: 10px;
    }
    .body {
        font-size: 15px;
        margin-bottom: 6px;
    }
    .btn {
        background-color: #007bff;
        color: #ffffff;
        padding: 10px 20px;
        border-radius: 6px;
        margin-top: 10px;
    }
    .btn-secondary {
        background-color: #334155;
        color: #ffffff;
        padding: 8px 14px;
        border-radius: 6px;
        margin-top: 10px;
    }
    .debug-title {
        font-size: 14px;
        color: #bfdbfe;
        margin-bottom: 8px;
    }
    .debug-log {
        font-size: 12px;
        color: #e2e8f0;
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
}

panel:root(class="screen") {
    panel:shell(class="shell") {
        panel:header(class="shell-header") {
            text:title(class="title", value="LDT Component Router")
            text:line1(class="body", value="Loaded from DSL and mounted into one persistent Scene")
            text:line2(class="body", value="The shell stays mounted while native page components swap inside #pageSlot")
            text:versionLine(class="body", value="Version __APP_VERSION__")
            button:btnFullscreen(id="btnFullscreen", class="btn") {
                text(value="Toggle Fullscreen", color="#ffffff")
            }
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

        panel:debug(class="debug-panel") {
            text:debugTitle(class="debug-title", value="Touch Debug Log")
            button:btnCopyLogs(id="btnCopyLogs", class="btn-secondary") {
                text(value="Copy Logs", color="#ffffff")
            }
            text:debugLogView(id="debugLogView", class="debug-log", value="waiting for touch logs...")
        }
    }
}

)ui";

    try
    {
        LDTParser parser;
        auto snippet = parser.parse(defaultSrc);
        auto ast = snippet.root();
        if (ast) {
            getDocumentRuntime()->notifyASTUpdated(ast);
        }
    }
    catch (const std::exception& ex)
    {
        LDT_ERROR("Initial parse error: " << ex.what());
    }

    onClick("#btnFullscreen", [this](const ldt::UIEventSystem::UIEvent&) {
        appendDebugLog("ui click target=btnFullscreen");
    });

    onClick("#btnNavHome", [this](const ldt::UIEventSystem::UIEvent&) {
        appendDebugLog("ui click target=btnNavHome");
        openPageSwitchDemo(PageSwitchPageKind::Home);
    });

    onClick("#btnNavProduct", [this](const ldt::UIEventSystem::UIEvent&) {
        appendDebugLog("ui click target=btnNavProduct");
        openPageSwitchDemo(PageSwitchPageKind::Product);
    });

    onClick("#btnNavSummary", [this](const ldt::UIEventSystem::UIEvent&) {
        appendDebugLog("ui click target=btnNavSummary");
        openPageSwitchDemo(PageSwitchPageKind::Summary);
    });

    onClick("#btnPageSwitchDemo", [this](const ldt::UIEventSystem::UIEvent&) {
        appendDebugLog("ui click target=btnPageSwitchDemo open=component-router-demo");
        openPageSwitchDemo(PageSwitchPageKind::Home);
    });

    onClick("#btnCopyLogs", [this](const ldt::UIEventSystem::UIEvent&) {
        copyDebugLogToClipboard();
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

    ldt::TestComponent testComp;
    testComp.onAttach(this, "test");
    appendDebugLog("debug log ready");

    return true;
}

void ExampleMainScene::onLogMessage(const LogMessage& message)
{
    if (message.text.empty()) {
        return;
    }

    if (message.isError) {
        appendDebugLog(std::string("ERROR ") + message.text);
        return;
    }

    appendDebugLog(message.text);
}

void ExampleMainScene::appendDebugLog(const std::string& message)
{
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

void ExampleMainScene::clearDebugLog()
{
    debugLogLines_.clear();
    nextLogSequence_ = 1;
    refreshDebugLogView();
}

void ExampleMainScene::copyDebugLogToClipboard()
{
    setClipboardString(joinLogLines(debugLogLines_));
    appendDebugLog("ui click target=btnCopyLogs copied=" + std::to_string(debugLogLines_.size()) + " lines");
}

void ExampleMainScene::refreshDebugLogView()
{
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

void ExampleMainScene::openPageSwitchDemo(PageSwitchPageKind pageKind)
{
    auto* runtime = getDocumentRuntime();
    if (!runtime) {
        return;
    }

    auto nextScene = std::make_shared<PageSwitchScene>(*runtime, pageKind);
    Stage::getInstance().replaceScene(nextScene);
}

} // namespace ldt
