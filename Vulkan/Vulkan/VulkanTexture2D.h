#pragma once

#include "Vulkan.h"
#include "VulkanImage.h"

#include "../Core/DataBuffer.h"
#include "../Renderer/RendererUtils.h"

struct Texture2DSpecifications
{
    FilterMode FilterMode = FilterMode::Bilinear;
    AddressMode AddressMode = AddressMode::Wrap;
    SamplesCount SamplesCount = SamplesCount::Samples1;
    float MaxAnisotropy = 1.f;
    bool bGenerateMips = false;
    bool bSRGB = true;
};

class VulkanSampler;
class VulkanTexture2D
{
public:
    VulkanTexture2D(const Path& path, const Texture2DSpecifications& specs = {});
    VulkanTexture2D(ImageFormat format, glm::uvec2 size, const void* data, const Texture2DSpecifications& specs);
    ~VulkanTexture2D();

    VulkanImage* GetImage() { return m_Image; }
    const VulkanImage* GetImage() const { return m_Image; }
    VulkanSampler* GetSampler() { return m_Sampler; }
    const VulkanSampler* GetSampler() const { return m_Sampler; }

    ImageFormat GetFormat() const { return m_Format; }
    glm::uvec2 GetSize() const { return { m_Width, m_Height }; }
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

private:
    bool Load(Path& path);

private:
    Texture2DSpecifications m_Specs;
    Path m_Path;
    DataBuffer m_ImageData;
    VulkanImage* m_Image = nullptr;
    VulkanSampler* m_Sampler = nullptr;
    ImageFormat m_Format = ImageFormat::Unknown;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
};
