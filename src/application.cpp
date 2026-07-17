// application.cpp
#include "application.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>

#include "misc/perf_timer.h"

#include "engine/Window.h"
#include "engine/window_platform_host.h"

#include "engine/core/parse.h"
#include "engine/core/ast_node.h"
#include "render/drawer.h"
#include "render/graphics_context.h"
#include "render/opengl_drawer_factory.h"

#include "render/display_list.h"
#include "engine/compositor.h"
#include "render/renderer.h"
#include "engine/style_engine.h"
#include "components/container_control.h"
#include "components/scene.h"
#include "components/control_manager.h"
#include "components/input.h"
#include "engine/box_model_engine.h"
#include "engine/layout_engine.h"
#include "engine/astbuild_engine.h"
#include "engine/resource_manager.h"
#include "engine/document_runtime.h"
#include "engine/view_coordinator.h"
#include "engine/update_scheduler.h"
#include "render/text_measurer.h"
#include <components/control_factory.h>
#include "components/stage.h"
using namespace ldt;

// 将 GLFW 窗口物理坐标线性映射为逻辑坐标 (DIP)
static void windowToLogical(GLFWwindow* window, double xd, double yd, float winW, float winH, float& outX, float& outY) {
    int ww, wh;
    Window::getWindowSize(window, &ww, &wh);
    outX = (ww > 0) ? static_cast<float>((xd / static_cast<double>(ww)) * static_cast<double>(winW)) : static_cast<float>(xd);
    outY = (wh > 0) ? static_cast<float>((yd / static_cast<double>(wh)) * static_cast<double>(winH)) : static_cast<float>(yd);
}

Application* Application::s_instance = nullptr;

Application::Application(std::string title, float winW, float winH)
    : m_winW(winW)
    , m_winH(winH)
    , m_Title(title)
    , g_documentRuntime(nullptr)
{
    s_instance = this;
}

Application::~Application()
{
    s_instance = nullptr;
}

static void registerAllParsers()
{
    ParserRegistry::instance().registerParser("import", std::make_shared<ImportParser>());
    ParserRegistry::instance().registerParser("style", std::make_shared<StyleParser>());
    ParserRegistry::instance().registerParser("layout", std::make_shared<LayoutParser>());
    ParserRegistry::instance().registerParser("content", std::make_shared<ContentParser>());
}

// Static wrappers that forward to the singleton instance
void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (!s_instance) return;
    
    // Physical size from GLFW (px)
    // Scale from GLFW (DPI)
    float sx, sy;
    Window::getWindowContentScale(window, &sx, &sy);

    const ldt::FramebufferSizePx framebufferSize{ width, height };
    const ldt::ContentScale contentScale{ { sx }, { sy } };
    const ldt::ViewportSizeDp viewportSize = ldt::toViewportSizeDp(framebufferSize, contentScale);
    
    // DPI size = Physical / Scale
    s_instance->m_winW = viewportSize.width.value;
    s_instance->m_winH = viewportSize.height.value;
    s_instance->m_scaleX = sx;
    s_instance->m_scaleY = sy;

    // ── Resize throttle ──
    // Eagerly resize the graphics context (GL state must be updated immediately),
    // but defer the expensive layout/render to the main loop (at most once per frame).
    if (s_instance->m_graphicsContext) {
        s_instance->m_graphicsContext->resize(framebufferSize, contentScale);
    }
    // Store latest viewport size and mark pending; main loop will consume it.
    s_instance->m_pendingViewportSize = viewportSize;
    s_instance->m_pendingResize = true;
}

void Application::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (!s_instance) return;

    Window::updateMouseCapture(window, button, action);

    double xd, yd;
    Window::getCursorPos(window, &xd, &yd);
    float x, y;
    windowToLogical(window, xd, yd, s_instance->m_winW, s_instance->m_winH, x, y);

    auto scene = Stage::getInstance().currentScene();
    if (scene)
    {
        (void)scene->onMouseButton({ { x }, { y } }, button, action);
    }
}

void Application::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!s_instance) return;
    double xd = xpos;
    double yd = ypos;
    Window::getCursorPos(window, &xd, &yd);
    float x, y;
    windowToLogical(window, xd, yd, s_instance->m_winW, s_instance->m_winH, x, y);

    auto scene = Stage::getInstance().currentScene();
    if (scene)
    {
        scene->onMouseMove({ { x }, { y } });
    }
}

void Application::cursor_enter_callback(GLFWwindow* window, int entered)
{
    if (!s_instance) return;
    auto scene = Stage::getInstance().currentScene();
    if (!scene) return;
    // entered == 0 means cursor left the window
    if (!entered && !Window::isMouseCaptured())
    {
        scene->clearPointerState();
    }
}

void Application::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (!s_instance) return;
    auto scene = Stage::getInstance().currentScene();
    if (scene)
    {
        scene->onScroll({ { static_cast<float>(xoffset) }, { static_cast<float>(yoffset) } });
    }
}

void Application::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_Q && action == GLFW_PRESS && (mods & GLFW_MOD_ALT)) {
        double xd, yd;
        Window::getCursorPos(window, &xd, &yd);
        float x, y;
        windowToLogical(window, xd, yd, s_instance->m_winW, s_instance->m_winH, x, y);

        auto scene = Stage::getInstance().currentScene();
        if (scene) {
            std::cout << "[Debug] Alt+Q pressed - Forcing onMouseMove at " << x << ", " << y << " (Window coords: " << xd << ", " << yd << ")" << std::endl;
            scene->onMouseMove({ { x }, { y } });
        }
    }
    //// Ctrl+F : request a full rebuild/refresh
    //if (key == GLFW_KEY_F && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
    //    if (s_instance->g_documentRuntime) {
    //        s_instance->g_documentRuntime->requestAllRebuild();
    //        LDT_LOG("Ctrl+F pressed - requested full rebuild");
    //    }
    //}
    if (!s_instance) return;
    auto scene = Stage::getInstance().currentScene();
    if (!scene) return;
    scene->onKey(key, scancode, action, mods);
}

void Application::char_callback(GLFWwindow* window, unsigned int codepoint)
{
    if (!s_instance) return;
    auto scene = Stage::getInstance().currentScene();
    if (!scene) return;
    scene->onChar(codepoint);
}

void Application::window_focus_callback(GLFWwindow* window, int focused)
{
    if (!s_instance) return;

    if (!focused)
    {
        Window::forceReleaseMouseCapture();
    }

    auto scene = Stage::getInstance().currentScene();
    if (!scene) return;
    scene->clearPointerState();
}

void Application::window_content_scale_callback(GLFWwindow* window, float xscale, float yscale)
{
    if (!s_instance) return;
    s_instance->m_scaleX = xscale;
    s_instance->m_scaleY = yscale;
    
    // Trigger a resize to recalculate DIP dimensions
    int w, h;
    Window::getFramebufferSize(window, &w, &h);
    Application::framebuffer_size_callback(window, w, h);
}

void Application::drop_callback(GLFWwindow* window, int count, const char** paths)
{
    (void)window;
    if (!s_instance) return;
    if (count <= 0) return;
    std::string path = paths && paths[0] ? paths[0] : std::string();
    if (path.empty()) return;

    // normalize extension
    std::string lower = path;
    for (auto &c : lower) c = (char)tolower(c);
    bool isText = false;
    if (lower.size() >= 4) {
        std::string ext = lower.substr(lower.size()-4);
        if (ext == ".ldt" || ext == ".txt") isText = true;
    }
    if (!isText && lower.size() >= 5) {
        if (lower.substr(lower.size()-5) == ".html") isText = true;
    }

    try {
        if (isText) {
            std::ifstream ifs(path);
            if (!ifs) return;
            std::stringstream ss; ss << ifs.rdbuf();
            std::string content = ss.str();
            LDTParser parser;
            auto snippet = parser.parse(content);
            std::shared_ptr<ASTNode> newAst = snippet.root();
            if (!newAst) return;
            s_instance->m_ast = newAst;
            if (s_instance->g_documentRuntime) s_instance->g_documentRuntime->notifyASTUpdated(newAst);
        } else {
            // image/resource: create minimal AST with image node
            auto root = ASTNode::make("Root");
            auto img = ASTNode::make("image");
            img->attrs["src"] = Attribute(path);
            root->children.push_back(img);
            s_instance->m_ast = root;
            if (s_instance->g_documentRuntime) s_instance->g_documentRuntime->notifyASTUpdated(root);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Drop parse/load error: " << ex.what() << std::endl;
    }
}

int Application::run(int argc, char* argv[])
{
    // Create window (Window constructor performs initialization)
    m_window = std::make_unique<Window>(static_cast<int>(m_winW), static_cast<int>(m_winH), m_Title, false);
    if (!m_window->get())
    {
        // init/creation failed
        return -1;
    }

    m_window->setFramebufferSizeCallback(Application::framebuffer_size_callback);
    m_window->setMouseButtonCallback(Application::mouse_button_callback);
    m_window->setCursorPosCallback(Application::cursor_pos_callback);
    m_window->setCursorEnterCallback(Application::cursor_enter_callback);
    m_window->setScrollCallback(Application::scroll_callback);
    m_window->setKeyCallback(Application::key_callback);
    m_window->setCharCallback(Application::char_callback);
    m_window->setWindowFocusCallback(Application::window_focus_callback);
    m_window->setWindowContentScaleCallback(Application::window_content_scale_callback);
    //m_window->setDropCallback(Application::drop_callback);

    // GLAD already loaded by Window::init()

    registerAllParsers();
    // Resource manager
    ResourceManager& resourceMgr = ResourceManager::getInstance();
    if (!resourceMgr.initialize())
    {
        std::cerr << "Failed to initialize ResourceManager\n";
        return -1;
    }
    resourceMgr.setDefaultFont("AlibabaPuHuiTi-3-35-Thin", 14.0f);
    
    // Drawer (use member graphics context like view)
    m_graphicsContext = createOpenGLGraphicsContext();
    int fbW, fbH;
    m_window->getFramebufferSize(&fbW, &fbH);
    m_graphicsContext->init(fbW, fbH);

    // Ensure we use the correct initial scale to set DIP dimensions
    m_window->getWindowContentScale(&m_scaleX, &m_scaleY);
    m_winW = static_cast<float>(fbW) / m_scaleX;
    m_winH = static_cast<float>(fbH) / m_scaleY;
    
    m_graphicsContext->resize({ fbW, fbH }, { { m_scaleX }, { m_scaleY } });

    // Text measurer: use drawer as ITextMeasurer implementation

    // Renderer & Compositor
    ldt::Renderer renderer(m_graphicsContext->getDrawer());
    m_compositor = std::make_unique<ldt::Compositor>();
    ldt::Compositor& compositor = *m_compositor;
    compositor.setRenderer(&renderer);

    // Engine context and services
    StyleEngine styleEngine;
    BoxModelEngine boxModelEngine;
    LayoutEngine layoutEngine;
    ASTBuildEngine astBuildEngine;
    DocumentRuntime documentRuntime;
    g_documentRuntime = &documentRuntime;

    documentRuntime.registerService(&styleEngine);
    documentRuntime.registerService(&layoutEngine);
    documentRuntime.registerService(&astBuildEngine);
    documentRuntime.registerService(&boxModelEngine);

    // 注入文本测量器（直接使用 drawer 实现）
    documentRuntime.setTextMeasurer(dynamic_cast<ITextMeasurer*>(m_graphicsContext->getDrawer()));

    documentRuntime.initializeAll({ typeid(StyleEngine), typeid(LayoutEngine), typeid(ASTBuildEngine), typeid(BoxModelEngine) });

    std::atomic<bool> pendingPresent{ false };
    m_host = std::make_unique<ldt::ViewCoordinator>(
        &compositor, &documentRuntime,
        ldt::ViewportSizeDp{ { m_winW }, { m_winH } },
        &pendingPresent,
        []() -> ldt::Scene* {
            auto scene = ldt::Stage::getInstance().currentScene();
            return scene ? scene.get() : nullptr;
        });
    WindowPlatformHost platformHost(m_window.get());
    // 将平台宿主注入到 Stage（所有 Scene 共享）
    ldt::Stage::getInstance().setPlatformHost(&platformHost);


    if (!build(documentRuntime, compositor, m_winW, m_winH)) {
        std::cerr << "Failed to build initial display list" << std::endl;
        return -1;
    }

    // Main loop (event + dirty-flag driven rendering)
    while (!m_window->shouldClose())
    {
        // 1) Process OS/window events (keep this call as required)
        m_window->pollEvents();

        auto currentScene = Stage::getInstance().currentScene();

        // 2) Advance animations (if any). If an animation needs advancing, it
        //    will enqueue a paint request via UpdateScheduler, then host handles it.
        bool hasAnimation = false;
        if (currentScene && currentScene->getControlManager())
        {
            hasAnimation = currentScene->getControlManager()->processAnimatedControls();
            if (hasAnimation)
            {
                UpdateScheduler::getInstance().requestPaint();
            }
        }

        // 3) Handle pending resize (throttled: at most once per frame)
        if (m_pendingResize) {
            m_pendingResize = false;
            if (m_host && g_documentRuntime) {
                PerfWatch _w("resize");
                m_host->handleResize(m_pendingViewportSize);
                // PerfWatch destructor auto-logs the total
            }
        }

        // 4) Handle scheduled layout updates
        auto updatePlan = UpdateScheduler::getInstance().consumePendingPlan();
        if (m_host) {
            m_host->apply(updatePlan);
        }

        // 3a) Handle deferred node removals
        ControlFactory::getInstance()->executePendingRemovals();

        // 4) If any subsystem requested a paint/repaint (input, resize, style change,
        //    animation), compositor will have received a DisplayList via the host
        //    and the host set `pendingPresent`.
        if (pendingPresent.exchange(false))
        {
            m_graphicsContext->clear(1, 1, 1, 1);
            compositor.paintAll();

            m_graphicsContext->present();
            m_window->swapBuffers();
            // continue immediately to process any newly queued events
            continue;
        }

        // 5) Idle: no dirty state and no animations — yield CPU without busy-waiting.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    m_graphicsContext->shutdown();
    // prefer instance-level quit (Window destructor will also call quit())
    if (m_window) m_window->quit();

    return 0;
}

bool Application::build(DocumentRuntime& runtime, ldt::Compositor& compositor, float width, float height)
{
    return true;
}

