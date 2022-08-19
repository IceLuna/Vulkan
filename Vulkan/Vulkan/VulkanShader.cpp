#include "VulkanShader.h"
#include "VulkanContext.h"

#include "../Core/FileSystem.h"

#include "shaderc/shaderc.hpp"
#include "spirv-tools/libspirv.h"
#include "spirv_cross/spirv_glsl.hpp"

#include <fstream>
#include <sstream>
#include <map>
#include <set>

namespace Utils
{
	static shaderc_env_version GetShaderCVersion()
	{
		switch (VulkanContext::GetVulkanAPIVersion())
		{
			case VK_API_VERSION_1_0: return shaderc_env_version_vulkan_1_0;
			case VK_API_VERSION_1_1: return shaderc_env_version_vulkan_1_1;
			case VK_API_VERSION_1_2: return shaderc_env_version_vulkan_1_2;
			case VK_API_VERSION_1_3: return shaderc_env_version_vulkan_1_3;
		}
		return shaderc_env_version_vulkan_1_0;
	}

	static shaderc_shader_kind VkShaderStageToShaderC(VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:    return shaderc_vertex_shader;
		case VK_SHADER_STAGE_FRAGMENT_BIT:  return shaderc_fragment_shader;
		case VK_SHADER_STAGE_COMPUTE_BIT:   return shaderc_compute_shader;
		}
		assert(false);
		return (shaderc_shader_kind)0;
	}

	static shaderc_shader_kind ShaderTypeToShaderC(ShaderType type)
	{
		switch (type)
		{
		case ShaderType::Vertex:    return shaderc_vertex_shader;
		case ShaderType::Fragment:  return shaderc_fragment_shader;
		case ShaderType::Compute:   return shaderc_compute_shader;
		}
		assert(false);
		return (shaderc_shader_kind)0;
	}

	static VkShaderStageFlagBits ShaderTypeToVK(ShaderType type)
	{
		switch (type)
		{
			case ShaderType::Vertex:    return VK_SHADER_STAGE_VERTEX_BIT;
			case ShaderType::Fragment:  return VK_SHADER_STAGE_FRAGMENT_BIT;
			case ShaderType::Geometry:  return VK_SHADER_STAGE_GEOMETRY_BIT;
			case ShaderType::Compute:   return VK_SHADER_STAGE_COMPUTE_BIT;
		}
		assert(false);
		return VK_SHADER_STAGE_ALL;
	}
	
	static VkFormat GLSLTypeToVulkan(const spirv_cross::SPIRType& type)
	{
		if (type.basetype == spirv_cross::SPIRType::Float)
		{
			switch (type.vecsize)
			{
				case 1: return VK_FORMAT_R32_SFLOAT;
				case 2: return VK_FORMAT_R32G32_SFLOAT;
				case 3: return VK_FORMAT_R32G32B32_SFLOAT;
				case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
			}
		}
		if (type.basetype == spirv_cross::SPIRType::Int)
		{
			switch (type.vecsize)
			{
				case 1: return VK_FORMAT_R32_SINT;
				case 2: return VK_FORMAT_R32G32_SINT;
				case 3: return VK_FORMAT_R32G32B32_SINT;
				case 4: return VK_FORMAT_R32G32B32A32_SINT;
			}
		}
		if (type.basetype == spirv_cross::SPIRType::UInt)
		{
			switch (type.vecsize)
			{
				case 1: return VK_FORMAT_R32_UINT;
				case 2: return VK_FORMAT_R32G32_UINT;
				case 3: return VK_FORMAT_R32G32B32_UINT;
				case 4: return VK_FORMAT_R32G32B32A32_UINT;
			}
		}
		if (type.basetype == spirv_cross::SPIRType::Half)
		{
			switch (type.vecsize)
			{
				case 1: return VK_FORMAT_R16_SFLOAT;
				case 2: return VK_FORMAT_R16G16_SFLOAT;
				case 3: return VK_FORMAT_R16G16B16_SFLOAT;
				case 4: return VK_FORMAT_R16G16B16A16_SFLOAT;
			}
		}
		assert(!"Unknown type");
		return VK_FORMAT_UNDEFINED;
	}

	struct GlslResource
	{
		const spirv_cross::Resource* Resource = nullptr;
		uint32_t Binding = 0;
		VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;

		bool operator< (const GlslResource& other) const
		{
			return Binding < other.Binding;
		}
	};

	// Set -> Resource
	static std::map<uint32_t, std::set<GlslResource>> GetDescriptorSets(const spirv_cross::Compiler& glsl, const spirv_cross::ShaderResources& resources)
	{
		std::map<uint32_t, std::set<GlslResource>> sets;

		for (auto& resource : resources.uniform_buffers)
		{
			uint32_t set     = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
			sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER });
		}
		for (auto& resource : resources.storage_buffers)
		{
			uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
			sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
		}
		for (auto& resource : resources.storage_images)
		{
			uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
			sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE });
		}
		for (auto& resource : resources.sampled_images)
		{
			uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
			sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER });
		}
		for (auto& resource : resources.separate_images)
		{
			uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
			sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
		}
		for (auto& resource : resources.separate_samplers)
		{
			uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
			sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_SAMPLER });
		}
		for (auto& resource : resources.acceleration_structures)
		{
			uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
			sets[set].insert({ &resource, binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR });
		}

		return sets;
	}
}

VulkanShader::VulkanShader(const std::filesystem::path& path, ShaderType shaderType, const ShaderDefines& defines)
	: m_Defines(defines)
	, m_Path(path)
	, m_Type(shaderType)
{
	Reload();
}

VulkanShader::~VulkanShader()
{
	VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
	if (m_ShaderModule)
	{
		vkDestroyShaderModule(device, m_ShaderModule, nullptr);
		m_ShaderModule = VK_NULL_HANDLE;
	}
}

void VulkanShader::Reload()
{
	LoadBinary();
	CreateShaderModule();
}

void VulkanShader::LoadBinary()
{
	std::ifstream fin(m_Path);
	if (!fin)
	{
		std::cout << "Failed to open shader file: " << m_Path << std::endl;
		return;
	}

	std::stringstream buffer;
	buffer << s_ShaderVersion << '\n';
	for (auto& define : m_Defines)
		buffer << "#define " << define.first << ' ' << define.second << '\n';
	buffer << fin.rdbuf();
	fin.close();
	std::string source = buffer.str();

	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	options.SetTargetEnvironment(shaderc_target_env_vulkan, Utils::GetShaderCVersion());
	options.SetWarningsAsErrors();
	options.SetGenerateDebugInfo();

	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, Utils::ShaderTypeToShaderC(m_Type), m_Path.u8string().c_str(), options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		std::cerr << "[Renderer::Vulkan] Failed to compile shader at: " << m_Path << '\n';
		std::cerr << "Error: \n" << module.GetErrorMessage();
		assert(false);
	}

	m_Binary = std::vector<uint32_t>(module.begin(), module.end());
	Reflect(m_Binary);
}

void VulkanShader::Reflect(const std::vector<uint32_t>& binary)
{
	spirv_cross::Compiler glsl(binary);
	spirv_cross::ShaderResources resources = glsl.get_shader_resources();
	VkShaderStageFlags vulkanShaderType = Utils::ShaderTypeToVK(m_Type);

	m_VertexAttribs.clear();
	m_LayoutBindings.clear();
	m_PushConstantRanges.clear();

	// Reflect vertex inputs
	if (m_Type == ShaderType::Vertex)
	{
		for (auto& input : resources.stage_inputs)
		{
			spirv_cross::SPIRType type = glsl.get_type(input.type_id);
			assert(type.width % 8 == 0);

			uint32_t size = (type.width / 8) * type.vecsize;

			auto& attrib = m_VertexAttribs.emplace_back();
			attrib.binding = 0;
			attrib.location = glsl.get_decoration(input.id, spv::DecorationLocation);
			attrib.offset = size;
			attrib.format = Utils::GLSLTypeToVulkan(type);
		}
	}
	
	// Reflect descriptor sets
	auto sets = Utils::GetDescriptorSets(glsl, resources);
	size_t setsCount = sets.size();
	if (setsCount > 0)
	{
		m_LayoutBindings.resize((--sets.end())->first + 1); // Extracting max value and resizing array
		size_t i = 0;
		for (auto& set : sets)
		{
			for (auto& resourceBinding : set.second)
			{
				auto& resource = resourceBinding.Resource;
				spirv_cross::SPIRType type = glsl.get_type(resource->type_id);

				auto& binding = m_LayoutBindings[i].emplace_back();
				binding.binding = resourceBinding.Binding;
				binding.descriptorCount = type.array.empty() ? 1u : type.array[0];
				binding.pImmutableSamplers = nullptr;
				binding.descriptorType = resourceBinding.DescriptorType;
				binding.stageFlags = vulkanShaderType;
			}
			++i;
		}
	}

	// Reflect push constants
	if (!resources.push_constant_buffers.empty())
	{
		auto ranges = glsl.get_active_buffer_ranges(resources.push_constant_buffers.front().id);

		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = vulkanShaderType;

		for (auto& range : ranges)
			pushConstantRange.size = std::max(pushConstantRange.size, uint32_t(range.offset + range.range));

		m_PushConstantRanges.push_back(pushConstantRange);
	}
}

void VulkanShader::CreateShaderModule()
{
	VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();
	if (m_ShaderModule)
	{
		vkDestroyShaderModule(device, m_ShaderModule, nullptr);
		m_ShaderModule = VK_NULL_HANDLE;
	}

	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = m_Binary.size() * sizeof(uint32_t);
	ci.pCode = m_Binary.data();

	VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &m_ShaderModule));

	VkPipelineShaderStageCreateInfo& shaderStage = m_PipelineShaderStageCI;
	shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.module = m_ShaderModule;
	shaderStage.pName = "main";
	shaderStage.stage = Utils::ShaderTypeToVK(m_Type);
}
