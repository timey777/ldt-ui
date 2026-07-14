// Window.cpp
#include "engine/Window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(HANDLE);
#endif

static GLFWcursor* s_arrow = nullptr;
static GLFWcursor* s_hand = nullptr;
static GLFWcursor* s_ibeam = nullptr;

#ifdef _WIN32
namespace {
unsigned int s_pressedButtonsMask = 0u;
bool s_isMouseCaptured = false;

const char* buttonName(int button)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT: return "Left";
    case GLFW_MOUSE_BUTTON_RIGHT: return "Right";
    case GLFW_MOUSE_BUTTON_MIDDLE: return "Middle";
    default: return "Other";
    }
}

const char* actionName(int action)
{
    switch (action)
    {
    case GLFW_PRESS: return "PRESS";
    case GLFW_RELEASE: return "RELEASE";
    default: return "OTHER";
    }
}

unsigned int toButtonMask(int button)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT: return 1u << 0;
    case GLFW_MOUSE_BUTTON_RIGHT: return 1u << 1;
    case GLFW_MOUSE_BUTTON_MIDDLE: return 1u << 2;
    default: return 0u;
    }
}
}
#endif

Window::Window(int width, int height, const std::string& title, bool isFullScreen)
:m_window(nullptr)
{
    if (!init(width, height, title, isFullScreen))
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
    }
}

Window::~Window()
{
    if (m_window)
        glfwDestroyWindow(m_window);
    quit();
}

bool Window::init(int width, int height, const std::string& title, bool isFullScreen)
{
#ifdef _WIN32
    // Enable DPI awareness before any window creation
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        PFN_SetProcessDpiAwarenessContext setDpi = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setDpi) {
            setDpi((HANDLE)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        }
    }
#endif

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWmonitor* pMonitor = isFullScreen ? glfwGetPrimaryMonitor() : NULL;
    m_window = glfwCreateWindow(width, height, title.c_str(), pMonitor, NULL);
    if (m_window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    glfwSetWindowPos(m_window, mode->width / 2 - width / 2, mode->height / 2 - height / 2);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    glfwSwapInterval(1);
    return true;
}

void Window::quit()
{
    glfwTerminate();
}

bool Window::shouldClose() const
{
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void Window::swapBuffers()
{
    if (m_window)
        glfwSwapBuffers(m_window);
}

void Window::pollEvents()
{
    glfwPollEvents();
}


void Window::setFramebufferSizeCallback(void (*cb)(GLFWwindow*, int, int))
{
    if (m_window)
        glfwSetFramebufferSizeCallback(m_window, cb);
}

void Window::setMouseButtonCallback(void (*cb)(GLFWwindow*, int, int, int))
{
    if (m_window)
        glfwSetMouseButtonCallback(m_window, cb);
}

void Window::setCursorPosCallback(void (*cb)(GLFWwindow*, double, double))
{
    if (m_window)
        glfwSetCursorPosCallback(m_window, cb);
}

void Window::setCursorEnterCallback(void (*cb)(GLFWwindow*, int))
{
    if (m_window)
        glfwSetCursorEnterCallback(m_window, cb);
}

void Window::setScrollCallback(void (*cb)(GLFWwindow*, double, double))
{
    if (m_window)
        glfwSetScrollCallback(m_window, cb);
}

void Window::setKeyCallback(void (*cb)(GLFWwindow*, int, int, int, int))
{
    if (m_window)
        glfwSetKeyCallback(m_window, cb);
}

void Window::setCharCallback(void (*cb)(GLFWwindow*, unsigned int))
{
    if (m_window)
        glfwSetCharCallback(m_window, cb);
}

void Window::setWindowFocusCallback(void (*cb)(GLFWwindow*, int))
{
    if (m_window)
        glfwSetWindowFocusCallback(m_window, cb);
}

void Window::setWindowContentScaleCallback(void (*cb)(GLFWwindow*, float, float))
{
    if (m_window)
        glfwSetWindowContentScaleCallback(m_window, cb);
}

void Window::getFramebufferSize(GLFWwindow* window, int* width, int* height)
{
    glfwGetFramebufferSize(window, width, height);
}

void Window::getWindowSize(GLFWwindow* window, int* width, int* height)
{
    glfwGetWindowSize(window, width, height);
}

void Window::getWindowContentScale(GLFWwindow* window, float* xscale, float* yscale)
{
    glfwGetWindowContentScale(window, xscale, yscale);
}

void Window::getFramebufferSize(int* width, int* height) const
{
    if (m_window)
        glfwGetFramebufferSize(m_window, width, height);
}

void Window::getWindowSize(int* width, int* height) const
{
    if (m_window)
        glfwGetWindowSize(m_window, width, height);
}

void Window::getWindowContentScale(float* xscale, float* yscale) const
{
    if (m_window)
        glfwGetWindowContentScale(m_window, xscale, yscale);
}

void Window::setDropCallback(void (*cb)(GLFWwindow*, int, const char**))
{
    if (m_window)
        glfwSetDropCallback(m_window, cb);
}

void Window::getCursorPos(GLFWwindow* win, double* x, double* y)
{
#ifdef _WIN32
    if (s_isMouseCaptured && win)
    {
        HWND hwnd = glfwGetWin32Window(win);
        if (hwnd)
        {
            POINT pt{};
            if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt))
            {
                if (x) *x = static_cast<double>(pt.x);
                if (y) *y = static_cast<double>(pt.y);
                return;
            }
        }
    }
#endif
    glfwGetCursorPos(win, x, y);
}

void Window::setCursor(GLFWwindow* win, CursorType type)
{
    if (!s_arrow) s_arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    if (!s_hand) s_hand = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    if (!s_ibeam) s_ibeam = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    GLFWcursor* target = s_arrow;
    if (type == CursorType::Hand) target = s_hand;
    else if (type == CursorType::IBeam) target = s_ibeam;
    glfwSetCursor(win, target);
}

void Window::updateMouseCapture(GLFWwindow* win, int button, int action)
{
#ifdef _WIN32
    const unsigned int mask = toButtonMask(button);
    if (mask == 0u || !win)
    {
        return;
    }

    if (action == GLFW_PRESS)
    {
        s_pressedButtonsMask |= mask;
        if (!s_isMouseCaptured)
        {
            HWND hwnd = glfwGetWin32Window(win);
            if (hwnd && SetCapture(hwnd))
            {
                s_isMouseCaptured = true;
            }
            else
            {
            }
        }
        return;
    }

    if (action == GLFW_RELEASE)
    {
        s_pressedButtonsMask &= ~mask;
        if (s_pressedButtonsMask == 0u && s_isMouseCaptured)
        {
            if (GetCapture() != nullptr)
            {
                ReleaseCapture();
            }
            s_isMouseCaptured = false;
        }
    }
#else
    (void)win;
    (void)button;
    (void)action;
#endif
}

bool Window::isMouseCaptured()
{
#ifdef _WIN32
    return s_isMouseCaptured;
#else
    return false;
#endif
}

void Window::forceReleaseMouseCapture()
{
#ifdef _WIN32
    s_pressedButtonsMask = 0u;
    if (s_isMouseCaptured)
    {
        if (GetCapture() != nullptr)
        {
            ReleaseCapture();
        }
        s_isMouseCaptured = false;
    }
#endif
}

void Window::setClipboardString(const std::string& s)
{
    if (m_window) glfwSetClipboardString(m_window, s.c_str());
}

std::string Window::getClipboardString() const
{
    if (!m_window) return std::string();
    const char* c = glfwGetClipboardString(m_window);
    return c ? std::string(c) : std::string();
}
