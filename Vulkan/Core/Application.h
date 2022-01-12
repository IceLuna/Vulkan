#pragma once

#include "Window.h"

class Application
{
public:
	Application(uint32_t width, uint32_t height, const std::string& title);
	~Application();

	Application(const Application&) = delete;
	Application& operator= (const Application&) = delete;

	static Application& GetApp() { return *s_App; }
	HWND GetWindowHandle() const { return m_Window.GetHandler(); }
	Window& GetWindow() { return m_Window; }

	void Run();

private:
	Window m_Window;
	bool m_Minimized = false;

	static Application* s_App;
};