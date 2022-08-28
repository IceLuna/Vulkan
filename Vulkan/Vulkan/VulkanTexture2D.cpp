#include "VulkanTexture2D.h"
#include "VulkanSampler.h"
#include "VulkanFence.h"
#include "VulkanCommandManager.h"

#include "../Renderer/Renderer.h"

#include "../stb_image.h"

#include <windows.h> // TODO: Remove

static ImageFormat ChannelsToFormat(int channels, bool bIsSRGB)
{
	switch (channels)
	{
		case 1: return bIsSRGB ? ImageFormat::R8_UNorm_SRGB			: ImageFormat::R8_UNorm;
		case 2: return bIsSRGB ? ImageFormat::R8G8_UNorm_SRGB		: ImageFormat::R8G8_UNorm;
		case 3: return bIsSRGB ? ImageFormat::R8G8B8_UNorm_SRGB		: ImageFormat::R8G8B8_UNorm;
		case 4: return bIsSRGB ? ImageFormat::R8G8B8A8_UNorm_SRGB	: ImageFormat::R8G8B8A8_UNorm;
	}
	assert(!"Invalid channels count");
	return ImageFormat::Unknown;
}
static ImageFormat HDRChannelsToFormat(int channels)
{
	switch (channels)
	{
		case 1: return ImageFormat::R32_Float;
		case 2: return ImageFormat::R32G32_Float;
		case 3: return ImageFormat::R32G32B32_Float;
		case 4: return ImageFormat::R32G32B32A32_Float;
	}
	assert(!"Invalid channels count");
	return ImageFormat::Unknown;
}

VulkanTexture2D::VulkanTexture2D(const Path& path, const Texture2DSpecifications& specs)
	: m_Specs(specs)
	, m_Path(path)
{
	bool bLoaded = Load(m_Path);
	if (bLoaded)
	{
		const uint32_t mipsCount = m_Specs.bGenerateMips ? CalculateMipCount(m_Width, m_Height) : 1;

		ImageSpecifications imageSpecs;
		imageSpecs.Size = glm::uvec3{ m_Width, m_Height, 1 };
		imageSpecs.Format = m_Format;
		imageSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst; // To sample in shader and to write texture data to it
		imageSpecs.Layout = ImageLayoutType::CopyDest; // Since we're about to write texture data to it
		imageSpecs.SamplesCount = m_Specs.SamplesCount;
		imageSpecs.MipsCount =  mipsCount;
		m_Image = new VulkanImage(imageSpecs);

		Ref<VulkanFence> writeFence = MakeRef<VulkanFence>();
		auto cmdManager = Renderer::GetGraphicsCommandManager();
		auto cmd = cmdManager->AllocateCommandBuffer();
		cmd.Write(m_Image, m_ImageData.Data, m_ImageData.Size, ImageLayoutType::CopyDest, ImageReadAccess::PixelShaderRead);
		cmd.End();
		cmdManager->Submit(&cmd, 1, writeFence, nullptr, 0, nullptr, 0);

		m_Sampler = new VulkanSampler(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, mipsCount > 1 ? float(mipsCount) : 0.f, m_Specs.MaxAnisotropy);

		writeFence->Wait();
	}
	else
	{
		assert(!"Failed to load texture");
		ImageSpecifications imageSpecs;
		imageSpecs.Size = glm::uvec3{ 1, 1, 1 };
		imageSpecs.Format = ImageFormat::Unknown;
		imageSpecs.Usage = ImageUsage::Sampled;
		imageSpecs.Layout = ImageLayoutType::Unknown;
		m_Image = new VulkanImage(VK_NULL_HANDLE, imageSpecs, true);
		m_Sampler = new VulkanSampler(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, 0.f, m_Specs.MaxAnisotropy);
	}
}

VulkanTexture2D::VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs)
	: m_Specs(specs)
	, m_Format(format)
	, m_Width(size.x)
	, m_Height(size.y)
{
	size_t dataSize = CalculateImageMemorySize(m_Format, m_Width, m_Height);
	const uint32_t mipsCount = m_Specs.bGenerateMips ? CalculateMipCount(m_Width, m_Height) : 1;

	if (data)
		m_ImageData = DataBuffer::Copy(data, dataSize);

	ImageSpecifications imageSpecs;
	imageSpecs.Format = m_Format;
	imageSpecs.Size = glm::uvec3{ m_Width, m_Height, 1 };
	imageSpecs.MipsCount = mipsCount;
	imageSpecs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;
	imageSpecs.Layout = ImageLayoutType::CopyDest; // Since we're about to write texture data to it
	imageSpecs.SamplesCount = m_Specs.SamplesCount;
	m_Image = new VulkanImage(imageSpecs);

	Ref<VulkanFence> writeFence = MakeRef<VulkanFence>();
	auto cmdManager = Renderer::GetGraphicsCommandManager();
	auto cmd = cmdManager->AllocateCommandBuffer();
	cmd.Write(m_Image, m_ImageData.Data, m_ImageData.Size, ImageLayoutType::CopyDest, ImageReadAccess::PixelShaderRead);
	cmd.End();
	cmdManager->Submit(&cmd, 1, writeFence, nullptr, 0, nullptr, 0);

	m_Sampler = new VulkanSampler(m_Specs.FilterMode, m_Specs.AddressMode, CompareOperation::Never, 0.f, mipsCount > 1 ? float(mipsCount) : 0.f, m_Specs.MaxAnisotropy);
	writeFence->Wait();
}

VulkanTexture2D::~VulkanTexture2D()
{
	if (m_Image)
	{
		delete m_Image;
		delete m_Sampler;
	}
	m_ImageData.Release();
}

bool VulkanTexture2D::Load(Path& path)
{
	int width, height, channels;

	std::wstring wPathString = m_Path.wstring();

	char cpath[2048];
	WideCharToMultiByte(65001 /* UTF8 */, 0, wPathString.c_str(), -1, cpath, 2048, NULL, NULL);

	if (stbi_is_hdr(cpath))
	{
		m_ImageData.Data = stbi_loadf(cpath, &width, &height, &channels, 0);
		m_Format = HDRChannelsToFormat(channels);
		m_ImageData.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
	}
	else
	{
		m_ImageData.Data = stbi_load(cpath, &width, &height, &channels, 0);
		m_Format = ChannelsToFormat(channels, m_Specs.bSRGB);
		m_ImageData.Size = CalculateImageMemorySize(m_Format, uint32_t(width), uint32_t(height));
	}

	assert(m_ImageData.Data); // Failed to load
	if (!m_ImageData.Data)
		return false;

	m_Width = (uint32_t)width;
	m_Height = (uint32_t)height;
	return true;
}
