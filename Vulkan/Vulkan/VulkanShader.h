#pragma once

#include "Vulkan.h"

#include <filesystem>
#include <vector>
#include <utility>

enum class ShaderType
{
	Vertex,
	Fragment,
	Geometry,
	Compute
};

using ShaderDefines = std::vector<std::pair<std::string, std::string>>;

class VulkanShader
{
public:
	VulkanShader(const std::filesystem::path& path, ShaderType shaderType, const ShaderDefines& defines = {});
	virtual ~VulkanShader();

	const VkPipelineShaderStageCreateInfo& GetPipelineShaderStageInfo() const { return m_PipelineShaderStageCI; }
	const std::vector<VkVertexInputAttributeDescription>& GetInputAttribs() const { return m_VertexAttribs; }

	const std::vector<VkPushConstantRange>& GetPushConstantRanges() const { return m_PushConstantRanges; }
	const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& GeLayoutSetBindings() const { return m_LayoutBindings; }

private:
	void Reload();
	void LoadBinary();
	void CreateShaderModule();
	void Reflect(const std::vector<uint32_t>& binary);

private:
	std::filesystem::path m_Path;
	ShaderDefines m_Defines;
	std::vector<VkVertexInputAttributeDescription> m_VertexAttribs;
	std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_LayoutBindings; // Set -> Bindings
	std::vector<VkPushConstantRange> m_PushConstantRanges;
	std::vector<uint32_t> m_Binary;
	VkShaderModule m_ShaderModule = VK_NULL_HANDLE;
	VkPipelineShaderStageCreateInfo m_PipelineShaderStageCI;
	ShaderType m_Type;

	static constexpr const char* s_ShaderVersion = "#version 450";
};
