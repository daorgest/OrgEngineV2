//
// Created by Orgest on 7/4/2025.
//

#include "VulkanShaderBuffer.h"

#include <algorithm>
#include "VulkanBuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "VulkanInit.h"
#include "VulkanPipeline.h"
#include "Tools/Logger.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

// Insertion sort :3
template<typename T>
static auto SortBindingsByBinding(Vector<T>& bindings) -> void
{
	for (u32 i = 1; i < bindings.size(); ++i)
	{
		const T key = bindings[i];
		u32 j = i;
		while (j > 0 && bindings[j - 1].binding > key.binding)
		{
			bindings[j] = bindings[j - 1];
			--j;
		}
		bindings[j] = key;
	}
}

VulkanShaderBuffer::VulkanShaderBuffer(GPUDevice* device, DescriptorAllocatorGrowable* alloc, const DescriptorSetLayoutDesc& desc)
{
	this->device = static_cast<VulkanDevice*>(device);
	this->allocator = alloc;
	this->desc = desc;

	// 1. Layout Construction
	SortBindingsByBinding(this->desc.bindings);

	DescriptorLayoutBuilder layoutBuilder;
	u32 maxBinding = 0;
	for (const auto& b : desc.bindings)
	{
		layoutBuilder.AddBinding(b);
		maxBinding = std::max(maxBinding, b.binding);

		// ONLY allocate buffers for actual buffer types
		if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
		{
		    slotCount++;
		}
	}

	// Builder now handles internal binding flags (bindless, stageFlags, etc.)
	layout = layoutBuilder.Build(this->device);

    // 2. Initialize Mapping Table
    const u32 bindingTableSize = maxBinding + 1;
    bindingToSlot.resize(bindingTableSize);
    for (u32 i = 0; i < bindingTableSize; ++i)
    {
        bindingToSlot[i] = UINT32_MAX;
    }

	// 3. Resource Allocation
	const u32 totalBufferCount = MAX_FRAME_OVERLAP * slotCount;
	buffers.resize(totalBufferCount);

    u32 currentSlot = 0;
	for (const auto& b : desc.bindings)
	{
	    if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
	    {
	        bindingToSlot[b.binding] = currentSlot;

	        for (u32 f = 0; f < MAX_FRAME_OVERLAP; ++f)
	        {
	            // Note: index() uses currentSlot via bindingToSlot
	            const u32 linearIndex = (f * slotCount) + currentSlot;
	            buffers[linearIndex] = VulkanBuffer(this->device, ToPreset(b.type), b.size);
	        }
	        currentSlot++;
	    }
	}
}

void VulkanShaderBuffer::UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) const
{
	ZoneScopedN("VulkanShaderBuffer::UpdateBinding");

	// Validate frame index
	if (frameIndex >= MAX_FRAME_OVERLAP)
	{
		LOG(Error, "ShaderBuffer: frameIndex {} out of range (max: {})", frameIndex, MAX_FRAME_OVERLAP);
		return;
	}

	// Validate binding -> slot mapping
	if (binding >= bindingToSlot.size())
	{
		LOG(Error, "ShaderBuffer: binding {} out of range (table size: {})", binding, bindingToSlot.size());
		return;
	}

	const u32 slot = bindingToSlot[binding];
	if (slot == UINT32_MAX)
	{
		LOG(Error, "ShaderBuffer: binding {} is not present in this UniformBufferDesc", binding);
		return;
	}

	if (slot >= slotCount)
	{
		LOG(Error, "ShaderBuffer: slot {} out of range (slotCount: {}) for binding={}", slot, slotCount, binding);
		return;
	}

	const u32 bufferIndex = index(frameIndex, binding);
	if (bufferIndex >= buffers.size())
	{
		LOG(Error, "ShaderBuffer: calculated buffer index {} out of range (size: {})", bufferIndex, buffers.size());
		return;
	}

	const VulkanBuffer& buf = buffers[bufferIndex];
	if (!buf.IsValid())
	{
		LOG(Error, "ShaderBuffer at frame={} binding={} (slot={}) is not valid!", frameIndex, binding, slot);
		return;
	}

	// Validate data
	if (data == nullptr)
	{
		LOG(Error, "ShaderBuffer: null data pointer for frame={} binding={}", frameIndex, binding);
		return;
	}

	// Validate size
	if (size > buf.info.size)
	{
		LOG(Error, "ShaderBuffer: data size {} exceeds buffer size {} for frame={} binding={}",
			size, buf.info.size, frameIndex, binding);
		return;
	}

	{
		ZoneScopedN("VulkanShaderBuffer Upload");
		buf.Upload(data, size);
	}
}

void VulkanShaderBuffer::Update(u32 frameIndex, const void* data, size_t size) const
{
	UpdateBinding(frameIndex, 0, data, size);
}

void VulkanShaderBuffer::Bind(GPUCommandBuffer* cmd, GPUPipeline* pipeline, u32 frameIndex) const
{
	ZoneScopedN("VulkanShaderBuffer::Bind");
	assert(frameIndex < MAX_FRAME_OVERLAP && "frameIndex out of range");

    cmd->BindDescriptorSet(&descriptorSets[frameIndex], desc.setIndex, pipeline);
}

void VulkanShaderBuffer::AllocateDescriptorSets(const bool isBindless, const u32 bindlessCount)
{
	ZoneScoped;
	DescriptorWriter writer;
	writer.writes.reserve(desc.bindings.size());

	for (u32 frame = 0; frame < MAX_FRAME_OVERLAP; ++frame)
	{
		const DescriptorSet set = allocator->Allocate(layout, isBindless, bindlessCount);
		descriptorSets[frame] = set;

		writer.Clear();
		for (const Binding& b : desc.bindings)
		{
			if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
			{
				const u32 bufferIdx = index(frame, b.binding);
				writer.WriteBuffer(b.binding, &buffers[bufferIdx], b.type);
			}
		}
		writer.UpdateSet(device, set);
	}
}

void VulkanShaderBuffer::Destroy()
{
	// Destroy all buffers
	for (auto& buf : buffers)
	{
		if (buf.IsValid())
		{
			buf.Destroy();
		}
	}
	buffers.clear();

	// Clear descriptor sets (Note: actual descriptor sets are freed by the allocator)
	for (auto& set : descriptorSets)
	{
		set.vk = VK_NULL_HANDLE;
	}

	// Destroy descriptor set layout
	layout.Destroy(device);

	// Clear pointers
	device       = nullptr;
	allocator    = nullptr;
	slotCount = 0;
	bindingToSlot.clear();
}