// Window.h
#pragma once

#include <string>
#include "ldt_export.h"

struct GLFWwindow;

class LDT_API Window {
public:
    enum class CursorType { Arrow, Hand, IBeam };

    Window(int width, int height, const std::string& title, bool isFullScreen = false);
    ~Window();

    // Platform init/terminate
    bool init(int width, int height, const std::string& title, bool isFullScreen);
    void quit();
    
    GLFWwindow* get() const { return m_window; }
    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();

    // Callback setters forward to GLFW inside Window.cpp
    void setFramebufferSizeCallback(void (*cb)(GLFWwindow*, int, int));
    void setMouseButtonCallback(void (*cb)(GLFWwindow*, int, int, int));
    void setCursorPosCallback(void (*cb)(GLFWwindow*, double, double));
    void setCursorEnterCallback(void (*cb)(GLFWwindow*, int));
    void setScrollCallback(void (*cb)(GLFWwindow*, double, double));
    void setKeyCallback(void (*cb)(GLFWwindow*, int, int, int, int));
    void setCharCallback(void (*cb)(GLFWwindow*, unsigned int));
    void setWindowFocusCallback(void (*cb)(GLFWwindow*, int));
    void setDropCallback(void (*cb)(GLFWwindow*, int, const char**));
    void setWindowContentScaleCallback(void (*cb)(GLFWwindow*, float, float));

    static void getFramebufferSize(GLFWwindow* window, int* width, int* height);
    static void getWindowContentScale(GLFWwindow* window, float* xscale, float* yscale);
    static void getWindowSize(GLFWwindow* window, int* width, int* height);
    void getFramebufferSize(int* width, int* height) const;
    void getWindowContentScale(float* xscale, float* yscale) const;
    void getWindowSize(int* width, int* height) const;

    static void getCursorPos(GLFWwindow* win, double* x, double* y);
    static void setCursor(GLFWwindow* win, CursorType type);
    static void updateMouseCapture(GLFWwindow* win, int button, int action);
    static bool isMouseCaptured();
    static void forceReleaseMouseCapture();

    // Clipboard helpers (wrap GLFW clipboard APIs)
    void setClipboardString(const std::string& s);
    std::string getClipboardString() const;

private:
    GLFWwindow* m_window;
};
