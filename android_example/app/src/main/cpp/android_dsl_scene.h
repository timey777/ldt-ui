#pragma once

#include <memory>
#include <string>
#include <vector>

#include "components/ui_component.h"
#include "components/scene.h"

extern "C" void dispatchToggleFullscreen();
void appendAndroidDebugLog(const std::string& message);
void clearAndroidDebugLog();
std::string loadAndroidAssetText(const std::string& assetPath);

enum class AndroidDemoPage {
    Home,
    Product,
    Summary,
};

class AndroidDslScene : public ldt::Scene {
public:
    using Scene::Scene;
    ~AndroidDslScene() override;

    bool setup() override;

    void setSource(std::string source);
    const std::string& source() const { return source_; }

    bool reload();
    void appendDebugLog(const std::string& message);
    void clearDebugLog();
    void copyDebugLogToClipboard();
    void refreshDebugLogView();

private:
    bool loadShellSource(const std::string& source);
    void bindShellEvents();
    void navigateTo(AndroidDemoPage nextPage, bool logNavigation = true);
    void clearActivePage();
    void updateShellState();

    std::string source_;
    std::vector<std::string> debugLogLines_;
    unsigned int nextLogSequence_ = 1;
    AndroidDemoPage currentPage_ = AndroidDemoPage::Home;
    std::unique_ptr<ldt::UIComponent> activePageComponent_;
    std::string activePageRootUid_;
    bool shellEventsBound_ = false;
};
