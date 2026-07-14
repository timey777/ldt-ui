#pragma once
#include <memory>
#include <stack>
#include "components/scene.h"
#include "engine/platform_host.h"
#include "ldt_export.h"

namespace ldt {

class LDT_API Stage {
public:
    static Stage& getInstance() {
        static Stage instance;
        return instance;
    }

    void pushScene(std::shared_ptr<Scene> scene) {
        scenes_.push(scene);
        scene->setup();
    }

    void replaceScene(std::shared_ptr<Scene> scene) {
        while (!scenes_.empty()) scenes_.pop();
        scenes_.push(scene);
        scene->setup();
    }

    void popScene() {
        if (!scenes_.empty()) scenes_.pop();
    }

    std::shared_ptr<Scene> currentScene() const {
        return scenes_.empty() ? nullptr : scenes_.top();
    }

    // 平台能力（光标、剪贴板）。Scene 只需告诉 Stage 做什么，不需要知道 IPlatformHost
    void setPlatformHost(IPlatformHost* h) { m_platformHost = h; }
    void setCursor(int cursorType) { if (m_platformHost) m_platformHost->setCursor(cursorType); }
    void setClipboardString(const std::string& s) { if (m_platformHost) m_platformHost->setClipboardString(s); }
    std::string getClipboardString() const { return m_platformHost ? m_platformHost->getClipboardString() : std::string(); }

private:
    Stage() = default;
    std::stack<std::shared_ptr<Scene>> scenes_;
    IPlatformHost* m_platformHost = nullptr;
};

} // namespace ldt
