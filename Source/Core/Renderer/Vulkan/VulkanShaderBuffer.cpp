//
// Created by Orgest on 7/4/2025.
//

#include "VulkanShaderBuffer.h"

#include <algorithm>
#include "VulkanBuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanInit.h"
#include "VulkanPipeline.h"
#include "Tools/Logger.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

static auto SortBindingsByBinding(std::span<Binding> bindings) -> void
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

VulkanShaderBuffer::VulkanShaderBuffer(VulkanDevice* dev, DescriptorAllocatorGrowable* alloc,
                                       const DescriptorSetLayoutDesc& desc)
{
	this->device = dev;
	this->allocator = alloc;
	this->desc = desc;

	// 1. Layout Construction
	SortBindingsByBinding(this->desc.bindings);

	DescriptorLayoutBuilder layoutBuilder;
	u32 maxBinding = 0;
	Vector<Binding> bufferOnlyBindings;

	for (const auto& b : desc.bindings)
	{
		layoutBuilder.AddBinding(b);
		maxBinding = std::max(maxBinding, b.binding);

		// ONLY allocate buffers for actual buffer types
		if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
		{
			bufferOnlyBindings.push_back(b);
		}
	}

	// Builder now handles internal binding flags (bindless, stageFlags, etc.)
	layout = layoutBuilder.Build(device);

	// 2. Slot Mapping
	slotCount = static_cast<u32>(bufferOnlyBindings.size());
	const u32 bindingTableSize = maxBinding + 1;
	bindingToSlot.resize(bindingTableSize);
	for (u32 i = 0; i < bindingTableSize; ++i) bindingToSlot[i] = UINT32_MAX;

	for (u32 slot = 0; slot < slotCount; ++slot)
	{
		bindingToSlot[bufferOnlyBindings[slot].binding] = slot;
	}

	// 3. Resource Allocation
	const u32 totalBufferCount = MAX_FRAME_OVERLAP * slotCount;
	buffers.resize(totalBufferCount);

	for (u32 f = 0; f < MAX_FRAME_OVERLAP; ++f)
	{
		for (u32 slot = 0; slot < slotCount; ++slot)
		{
			const Binding& b = desc.bindings[slot];
			const u32 linearIndex = (f * slotCount) + slot;

			buffers[linearIndex] = VulkanBuffer(device, ToPreset(b.type), b.size);
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

void VulkanShaderBuffer::Bind(GPUCommandBuffer* cmd, const VulkanPipeline& pipeline, u32 frameIndex) const
{
	ZoneScopedN("VulkanShaderBuffer::Bind");
	assert(frameIndex < MAX_FRAME_OVERLAP && "frameIndex out of range");

	const auto* vkCmd = static_cast<VulkanCommandBuffer*>(cmd);
	const DescriptorSet set = descriptorSets[frameIndex];
	vkCmdBindDescriptorSets(vkCmd->GetVkHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vkLayout, desc.setIndex, 1, &set.vk, 0, nullptr);
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