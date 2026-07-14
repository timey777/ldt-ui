#pragma once

#include "platform_host.h"
#include "Window.h"

// 基于 GLFW Window 的 IPlatformHost 实现
// 用于桌面端（ldt-ui 等），由 Application 创建并注入到 Stage
struct WindowPlatformHost : public ldt::IPlatformHost {
	Window* window;

	explicit WindowPlatformHost(Window* w) : window(w) {}

	void setCursor(int cursorType) override {
		if (!window) return;
		// Cursor_Arrow=0, Cursor_Hand=1, Cursor_IBeam=2 (mirrors ldt::Scene::CursorType)
		Window::CursorType t = Window::CursorType::Arrow;
		if (cursorType == 1) t = Window::CursorType::Hand;
		else if (cursorType == 2) t = Window::CursorType::IBeam;
		Window::setCursor(window->get(), t);
	}

	void setClipboardString(const std::string& s) override {
		if (window) window->setClipboardString(s);
	}

	std::string getClipboardString() override {
		return window ? window->getClipboardString() : std::string();
	}
};
