#include "DescriptorSetData.h"

void DescriptorSetData::SetArg(std::uint32_t idx, const VulkanBuffer* buffer)
{
    auto& currentBinding = m_Bindings[idx];

    BufferBinding binding(buffer);
    if (currentBinding.BufferBindings[0] != binding)
    {
        currentBinding.BufferBindings[0] = binding;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArg(std::uint32_t idx, const VulkanBuffer* buffer, std::size_t offset, std::size_t size)
{
    auto& currentBinding = m_Bindings[idx];

    BufferBinding binding(buffer, offset, size);
    if (currentBinding.BufferBindings[0] != binding)
    {
        currentBinding.BufferBindings[0] = binding;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArg(std::uint32_t idx, const VulkanImage* image)
{
    SetArg(idx, image, nullptr);
}

void DescriptorSetData::SetArg(std::uint32_t idx, const VulkanImage* image, const ImageView& imageView)
{
    SetArg(idx, image, imageView, nullptr);
}

void DescriptorSetData::SetArg(std::uint32_t idx, const VulkanImage* image, const VulkanSampler* sampler)
{
    auto& currentBinding = m_Bindings[idx];

    ImageBinding binding(image, sampler);
    if (currentBinding.ImageBindings[0] != binding)
    {
        currentBinding.ImageBindings[0] = binding;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArg(std::uint32_t idx, const VulkanImage* image, const ImageView& imageView, const VulkanSampler* sampler)
{
    auto& currentBinding = m_Bindings[idx];

    ImageBinding binding(image, imageView, sampler);
    if (currentBinding.ImageBindings[0] != binding)
    {
        currentBinding.ImageBindings[0] = binding;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArgArray(std::uint32_t idx, const std::vector<const VulkanBuffer*>& buffers)
{
    assert(buffers.size());
    auto& currentBinding = m_Bindings[idx];

    std::vector<BufferBinding> bindings;
    bindings.reserve(buffers.size());

    for (auto& buffer : buffers)
        bindings.emplace_back(buffer);

    if (currentBinding.BufferBindings != bindings)
    {
        currentBinding.BufferBindings = bindings;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images)
{
    assert(images.size());
    auto& currentBinding = m_Bindings[idx];

    std::vector<ImageBinding> bindings;
    bindings.reserve(images.size());

    for (auto& image : images)
        bindings.emplace_back(image);

    if (currentBinding.ImageBindings != bindings)
    {
        currentBinding.ImageBindings = bindings;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews)
{
    const size_t imagesCount = images.size();
    assert(imagesCount);
    auto& currentBinding = m_Bindings[idx];

    std::vector<ImageBinding> bindings;
    bindings.reserve(imagesCount);

    for (size_t i = 0; i < imagesCount; ++i)
        bindings.emplace_back(images[i], imageViews[i]);

    if (currentBinding.ImageBindings != bindings)
    {
        currentBinding.ImageBindings = bindings;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<const VulkanSampler*>& samplers)
{
    const size_t imagesCount = images.size();
    assert(imagesCount);
    auto& currentBinding = m_Bindings[idx];

    std::vector<ImageBinding> bindings;
    bindings.reserve(imagesCount);

    for (size_t i = 0; i < imagesCount; ++i)
        bindings.emplace_back(images[i], samplers[i]);

    if (currentBinding.ImageBindings != bindings)
    {
        currentBinding.ImageBindings = bindings;
        m_bDirty = true;
    }
}

void DescriptorSetData::SetArgArray(std::uint32_t idx, const std::vector<const VulkanImage*>& images, const std::vector<ImageView>& imageViews, const std::vector<const VulkanSampler*>& samplers)
{
    const size_t imagesCount = images.size();
    assert(imagesCount);
    auto& currentBinding = m_Bindings[idx];

    std::vector<ImageBinding> bindings;
    bindings.reserve(imagesCount);

    for (size_t i = 0; i < imagesCount; ++i)
        bindings.emplace_back(images[i], imageViews[i], samplers[i]);

    if (currentBinding.ImageBindings != bindings)
    {
        currentBinding.ImageBindings = bindings;
        m_bDirty = true;
    }
}
