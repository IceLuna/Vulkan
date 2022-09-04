#include "VulkanDescriptorManager.h"
#include "VulkanContext.h"
#include "VulkanPipeline.h"

#include <iostream>
#include <string>

static constexpr uint32_t s_MaxSets = 40960u;
static constexpr uint32_t s_NumDescriptors = 81920u;

static VkDevice s_Device = VK_NULL_HANDLE;
static VkDescriptorPool s_DescriptorPool = VK_NULL_HANDLE;

//-------------------
// DESCRIPTOR MANAGER
//-------------------
void VulkanDescriptorManager::Init()
{
    constexpr VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_NumDescriptors },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_NumDescriptors },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, s_NumDescriptors },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_NumDescriptors },
        { VK_DESCRIPTOR_TYPE_SAMPLER, s_NumDescriptors },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, s_NumDescriptors }
    };

    assert(!s_Device);
    s_Device = VulkanContext::GetDevice()->GetVulkanDevice();

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
    info.pPoolSizes = poolSizes;
    info.maxSets = s_MaxSets;
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    VK_CHECK(vkCreateDescriptorPool(s_Device, &info, nullptr, &s_DescriptorPool));
}

void VulkanDescriptorManager::Shutdown()
{
    if (s_DescriptorPool)
        vkDestroyDescriptorPool(s_Device, s_DescriptorPool, nullptr);

    s_DescriptorPool = VK_NULL_HANDLE;
    s_Device = VK_NULL_HANDLE;
}

VulkanDescriptorSet VulkanDescriptorManager::AllocateDescriptorSet(const VulkanPipeline* pipeline, uint32_t set)
{
    return VulkanDescriptorSet(pipeline, s_DescriptorPool, set);
}

void VulkanDescriptorManager::WriteDescriptors(const VulkanPipeline* pipeline, const std::vector<DescriptorWriteData>& writeDatas)
{
    std::vector<VkDescriptorBufferInfo> buffers;
    std::vector<VkDescriptorImageInfo> images;
    std::vector<VkWriteDescriptorSet> vkWriteDescriptorSets;
    size_t buffersInfoCount = 0;
    size_t imagesInfoCount = 0;

    for (auto& writeData : writeDatas)
    {
        uint32_t set = writeData.DescriptorSet->GetSetIndex();
        const auto& setBindings = pipeline->GetSetBindings(set);
        const auto& setBindingsData = writeData.DescriptorSetData->GetBindings();

        for (auto& binding : setBindings)
        {
            const auto& bindingData = setBindingsData.at(binding.binding);
            
            if (IsBufferType(binding.descriptorType))
                buffersInfoCount += binding.descriptorCount;
            else if (IsImageType(binding.descriptorType))
                imagesInfoCount += binding.descriptorCount;
            else if (IsSamplerType(binding.descriptorType))
                imagesInfoCount += bindingData.ImageBindings[0].Sampler ? 1 : 0;
        }
    }

    buffers.reserve(buffersInfoCount);
    images.reserve(imagesInfoCount);

    for (auto& writeData : writeDatas)
    {
        uint32_t set = writeData.DescriptorSet->GetSetIndex();
        const auto& setBindings = pipeline->GetSetBindings(set);
        const auto& setBindingsData = writeData.DescriptorSetData->GetBindings();

        for (auto& binding : setBindings)
        {
            const auto& bindingData = setBindingsData.at(binding.binding);

            if (IsBufferType(binding.descriptorType))
            {
                if (bindingData.BufferBindings.empty())
                    continue;

                const size_t startIdx = buffers.size();
                for (auto& buffer : bindingData.BufferBindings)
                {
                    VkBuffer vkBuffer = buffer.Buffer;
                    if (vkBuffer == VK_NULL_HANDLE)
                        std::cerr << "Error! Invalid buffer binding for binding " << std::to_string(binding.binding) << ", set " << std::to_string(set) << '\n';

                    buffers.push_back({ vkBuffer, buffer.Offset, buffer.Range });
                }
                
                uint32_t descriptorCount = std::min(binding.descriptorCount, (uint32_t)bindingData.BufferBindings.size());
                VkWriteDescriptorSet& writeDescriptorSet = vkWriteDescriptorSets.emplace_back();
                writeDescriptorSet = {};
                writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.descriptorCount = descriptorCount;
                writeDescriptorSet.descriptorType = binding.descriptorType;
                writeDescriptorSet.dstBinding = binding.binding;
                writeDescriptorSet.dstSet = writeData.DescriptorSet->GetVulkanDescriptorSet();
                writeDescriptorSet.pBufferInfo = &buffers[startIdx];
            }
            else if (IsImageType(binding.descriptorType) || IsSamplerType(binding.descriptorType))
            {
                if (bindingData.ImageBindings.empty())
                    continue;

                const size_t startIdx = images.size();
                if (IsSamplerType(binding.descriptorType))
                {
                    images.push_back({ bindingData.ImageBindings[0].Sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED });
                }
                else
                {
                    for (auto& image : bindingData.ImageBindings)
                    {
                        VkSampler sampler = image.Sampler;
                        VkImageView imageView = image.View;
                        VkImageLayout imageLayout;
                        if ((binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) || (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
                            imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        else
                            imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                        images.push_back({ sampler, imageView, imageLayout });
                    }
                }

                uint32_t descriptorCount = std::min(binding.descriptorCount, (uint32_t)bindingData.ImageBindings.size());
                VkWriteDescriptorSet& writeDescriptorSet = vkWriteDescriptorSets.emplace_back();
                writeDescriptorSet = {};
                writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSet.descriptorCount = descriptorCount;
                writeDescriptorSet.descriptorType = binding.descriptorType;
                writeDescriptorSet.dstBinding = binding.binding;
                writeDescriptorSet.dstSet = writeData.DescriptorSet->GetVulkanDescriptorSet();
                writeDescriptorSet.pImageInfo = &images[startIdx];
            }
            else
            {
                std::cerr << "Unknown binding\n";
                assert(false);
            }
        }

        writeData.DescriptorSetData->OnFlushed();
    }

    vkUpdateDescriptorSets(s_Device, (uint32_t)writeDatas.size(), vkWriteDescriptorSets.data(), 0, nullptr);
}

//-------------------
//  DESCRIPTOR SET
//-------------------
VulkanDescriptorSet::VulkanDescriptorSet(const VulkanPipeline* pipeline, VkDescriptorPool pool, uint32_t set)
    : m_Device(VulkanContext::GetDevice()->GetVulkanDevice())
    , m_DescriptorPool(pool)
    , m_SetIndex(set)
{
    VkDescriptorSetLayout setLayout = pipeline->GetDescriptorSetLayout(set);

    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &setLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_Device, &info, &m_DescriptorSet));
}
