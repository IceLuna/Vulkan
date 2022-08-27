#include "VulkanStagingManager.h"

#include <vector>

//------------------
// Staging Manager
//------------------

static std::vector<VulkanStagingBuffer*> s_StagingBuffers;

VulkanStagingBuffer* VulkanStagingManager::AcquireBuffer(size_t size, bool bIsCPURead)
{
	VulkanStagingBuffer* stagingBuffer = nullptr;

	for (auto& staging : s_StagingBuffers)
	{
		assert(staging->m_Fence);
		if (staging->IsCPURead() == bIsCPURead && size <= staging->GetSize())
		{
			if (staging->m_State == StagingBufferState::Free)
			{
				stagingBuffer = staging;
				break;
			}
			else if (staging->m_State == StagingBufferState::InFlight)
			{
				if (staging->m_Fence->IsSignaled())
				{
					stagingBuffer = staging;
					break;
				}
			}
		}
	}

	if (!stagingBuffer)
	{
		stagingBuffer = new VulkanStagingBuffer(size, bIsCPURead);
		s_StagingBuffers.push_back(stagingBuffer);
	}
	stagingBuffer->m_State = StagingBufferState::Pending;

	return stagingBuffer;
}

void VulkanStagingManager::ReleaseBuffers()
{
	auto it = s_StagingBuffers.begin();
	while (it != s_StagingBuffers.end())
	{
		if ((*it)->m_State == StagingBufferState::Free)
			it = s_StagingBuffers.erase(it);
		else if ((*it)->m_State == StagingBufferState::InFlight && (*it)->m_Fence->IsSignaled())
		{
			delete (*it);
			it = s_StagingBuffers.erase(it);
		}
		else
			++it;
	}
}

//------------------
// Staging Buffer
//------------------
VulkanStagingBuffer::VulkanStagingBuffer(size_t size, bool bIsCPURead)
	: m_bIsCPURead(bIsCPURead)
{
	BufferSpecifications specs;
	specs.Size = size;
	specs.MemoryType = bIsCPURead ? MemoryType::GpuToCpu : MemoryType::CpuToGpu;
	specs.Usage = bIsCPURead ? BufferUsage::TransferDst : BufferUsage::TransferSrc;

	m_Buffer = new VulkanBuffer(specs);
}
