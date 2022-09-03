#pragma once

class VulkanContext;
class VulkanCommandManager;
class VulkanCommandBuffer;

class Renderer
{
private:
	static void BeginImGui();
	static void DrawImGui();
	static void EndImGui(VulkanCommandBuffer* cmd);

public:
	static void Init();
	static void Shutdown();
	static void DrawFrame(float ts);
	static void OnWindowResized();

	static VulkanContext& GetContext();
	static VulkanCommandManager* GetGraphicsCommandManager();

	static const char* GetRendererCachePath() { return "Cache/Renderer"; }
};
