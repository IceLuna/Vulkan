#pragma once

class Renderer
{
public:
	static void Init();
	static void Shutdown();
	static void DrawFrame();
	static void OnWindowResized();

	static void BeginImGui();
	static void EndImGui();
};
