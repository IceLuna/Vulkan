#pragma once

class VulkanContext;

class Renderer
{
public:
	static void Init(unsigned int width, unsigned int height);
	static void Shutdown();
	static void DrawFrame();
	static void OnWindowResized();
	static void BeginImGui();
	static VulkanContext& GetContext();
	static void EndImGui();

	static const char* GetPipelineCachePath() { return "Cache/Renderer"; }
};
