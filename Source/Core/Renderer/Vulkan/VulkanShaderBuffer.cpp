//
// Created by Orgest on 7/4/2025.
//

#include "VulkanShaderBuffer.h"

#include <algorithm>
#include "VulkanBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanInit.h"
#include "VulkanPipeline.h"
#include "Tools/Logger.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

static void SortBindingsByBinding(Vector<Binding>& bindings)
{
	const u32 n = bindings.size();
	for (u32 i = 1; i < n; ++i)
	{
		const Binding key = bindings[i];
		u32 j = i;
		while (j > 0 && bindings[j - 1].binding > key.binding)
		{
			bindings[j] = bindings[j - 1];
			--j;
		}
		bindings[j] = key;
	}
}

VulkanShaderBuffer::VulkanShaderBuffer(VulkanDevice* dev, DescriptorAllocatorGrowable* alloc, const UniformBufferDesc& desc)
{
	this->device    = dev;
	this->allocator = alloc;
	this->desc      = desc;

	bool needsSort = false;
	for (u32 i = 1; i < this->desc.bindings.size(); i++)
	{
		if (this->desc.bindings[i - 1].binding > this->desc.bindings[i].binding)
		{
			needsSort = true;
			break;
		}
	}
	if (needsSort)
		SortBindingsByBinding(this->desc.bindings);

	DescriptorLayoutBuilder layoutBuilder;
	u32                     maxBinding = 0;

	for (const auto& b : desc.bindings)
	{
		assert((b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
			&& "VulkanShaderBuffer only supports uniform and storage buffers!");
		layoutBuilder.AddBinding(b.binding, b.type);
		maxBinding = std::max(maxBinding, b.binding);
	}

	layout = layoutBuilder.Build(device->device, desc.stageFlags);

	// Build binding -> slot mapping
	slotCount = static_cast<u32>(this->desc.bindings.size());
	const u32 bindingTableSize = maxBinding + 1;

	bindingToSlot.resize(bindingTableSize);
	for (u32 i = 0; i < bindingTableSize; ++i)
	{
		bindingToSlot[i] = UINT32_MAX; // mark as "not present"
	}

	for (u32 slot = 0; slot < slotCount; ++slot)
	{
		const Binding& b = this->desc.bindings[slot];
		assert(b.binding < bindingToSlot.size());
		bindingToSlot[b.binding] = slot;
	}

	// Each binding needs its own buffer for every frame-in-flight.
	// Example: 5 bindings and 2 frames -> 10 total buffers.
	// Layout is: [frame0 slots][frame1 slots][...]
	const u32 total = MAX_FRAME_OVERLAP * slotCount;
	buffers.resize(total);

	for (u32 f = 0; f < MAX_FRAME_OVERLAP; ++f)
	{
		for (const auto& [binding, type, size] : desc.bindings)
		{
			const u32 i = index(f, binding);
			buffers[i] = VulkanBuffer(device, ToPreset(type), size);

			if (!buffers[i].IsValid())
			{
				LOG(Error, "Failed to create buffer for frame={} binding={}", f, binding);
			}
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

void VulkanShaderBuffer::Bind(VkCommandBuffer cmd, const VulkanPipeline& pipeline, u32 frameIndex) const
{
	ZoneScopedN("VulkanShaderBuffer::Bind");
	assert(frameIndex < MAX_FRAME_OVERLAP && "frameIndex out of range");
	const DescriptorSet set = descriptorSets[frameIndex];
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vkLayout, desc.setIndex, 1, &set.vk, 0, nullptr);
}

void VulkanShaderBuffer::AllocateDescriptorSets()
{
	ZoneScoped;
	DescriptorWriter writer;
	writer.writes.reserve(this->desc.bindings.size());

	for (u32 frame = 0; frame < MAX_FRAME_OVERLAP; ++frame)
	{
		ZoneScopedN("Allocating descriptor sets");
		const DescriptorSet set = allocator->Allocate(layout);
		if (set.vk == VK_NULL_HANDLE)
		{
			LOG(Error, "Failed to allocate descriptor set for frame {}", frame);
			continue;
		}
		descriptorSets[frame] = set;

		writer.Clear();

		for (const Binding& b : this->desc.bindings)
		{
			if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
			{
				const u32 bufferIdx = index(frame, b.binding);
				if (bufferIdx >= buffers.size())
				{
					LOG(Error, "ShaderBuffer: buffer index {} out of range (size: {}) for frame={} binding={}",
						bufferIdx, buffers.size(), frame, b.binding);
					continue;
				}

				const VulkanBuffer& buf = buffers[bufferIdx];
				if (!buf.IsValid())
				{
					LOG(Error, "Invalid buffer at frame={} binding={}", frame, b.binding);
					continue;
				}
				writer.WriteBuffer(b.binding, &buf, b.type);
			}
		}

		writer.UpdateSet(device->device, set.vk);
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
	if (layout.vk != VK_NULL_HANDLE && device != nullptr && device->device != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device->device, layout.vk, nullptr);
		layout.vk = VK_NULL_HANDLE;
	}

	// Clear pointers
	device       = nullptr;
	allocator    = nullptr;
	slotCount = 0;
	bindingToSlot.clear();
}