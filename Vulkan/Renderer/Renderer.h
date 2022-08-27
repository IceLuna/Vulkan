#pragma once

class VulkanContext;
class VulkanCommandManager;

class Renderer
{
public:
	static void Init();
	static void Shutdown();
	static void DrawFrame();
	static void OnWindowResized();
	static void BeginImGui();
	static void EndImGui();

	static VulkanContext& GetContext();
	static VulkanCommandManager* GetGraphicsCommandManager();

	static const char* GetPipelineCachePath() { return "Cache/Renderer"; }
};
