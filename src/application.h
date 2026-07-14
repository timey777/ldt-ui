// Application.h
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "ldt_export.h"
#include "engine/core/coordinate_types.h"
struct GLFWwindow;

class Window;

struct ASTNode;

namespace ldt {
    class Scene;
    class Compositor;
    class ViewCoordinator;
    class Panel;
    class Input;
    class MainScene;
    class DisplayList;
}
class DocumentRuntime;
class LDT_API Application {
public:
    Application(std::string title,
    float winW,
    float winH);
    ~Application();
    int run(int argc, char* argv[]);

    // GLFW callback wrappers
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void cursor_enter_callback(GLFWwindow* window, int entered);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void char_callback(GLFWwindow* window, unsigned int codepoint);
    static void window_focus_callback(GLFWwindow* window, int focused);
    static void window_content_scale_callback(GLFWwindow* window, float xscale, float yscale);
    static void drop_callback(GLFWwindow* window, int count, const char** paths);

private:
    static Application* s_instance;
    // non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    std::string m_Title;
    float m_winW;
    float m_winH;
    float m_scaleX{ 1.0f };
    float m_scaleY{ 1.0f };
    DocumentRuntime* g_documentRuntime;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<class IGraphicsContext> m_graphicsContext;
    std::shared_ptr<ASTNode> m_ast;
    std::unique_ptr<ldt::Compositor> m_compositor;
    std::unique_ptr<ldt::ViewCoordinator> m_host;

    // ── Resize throttle: defer layout to main loop (at most once per frame) ──
    bool m_pendingResize = false;
    ldt::ViewportSizeDp m_pendingViewportSize{ { 0.0f }, { 0.0f } };

protected:
    // Encapsulate full initial render pipeline: AST -> render tree -> controls -> display list
    // Returns true on success and fills `outDisplayList`.
    virtual bool build(DocumentRuntime& runtime, ldt::Compositor& compositor, float width, float height);
};
