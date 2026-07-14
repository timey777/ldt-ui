// MainScene.h
#pragma once

#include "components/scene.h"
#include "engine/document_runtime.h"
#include "engine/compositor.h"
#include "misc/logger.h"
#include "PageSwitchScene.h"

#include <memory>
#include <string>
#include <vector>

namespace ldt {
    class Scene;
    class Input;

    class ExampleMainScene : public Scene, public ILogSink {
    public:
        explicit ExampleMainScene(DocumentRuntime& runtime);
        ~ExampleMainScene() override;

        bool setup() override;

        void onLogMessage(const LogMessage& message) override;

    private:
        void appendDebugLog(const std::string& message);
        void clearDebugLog();
        void copyDebugLogToClipboard();
        void refreshDebugLogView();
        void openPageSwitchDemo(PageSwitchPageKind pageKind = PageSwitchPageKind::Home);

        std::vector<std::string> debugLogLines_;
        unsigned int nextLogSequence_ = 1;
        LogSinkSubscription logSubscription_;
    };
}
