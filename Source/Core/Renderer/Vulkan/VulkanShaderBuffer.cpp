//
// Created by Orgest on 7/4/2025.
//

#include "VulkanShaderBuffer.h"

#include <ranges>

#include "VulkanBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanInit.h"
#include "VulkanPipeline.h"
#include "Tools/Logger.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

VulkanShaderBuffer::VulkanShaderBuffer(VulkanDevice* dev, DescriptorAllocatorGrowable* alloc, const UniformBufferDesc& desc)
{
	this->device = dev;
	this->allocator = alloc;
	this->desc = desc;

	DescriptorLayoutBuilder layoutBuilder;
	u32 maxBinding = 0;

	for (const auto& b : desc.bindings)
	{
		assert((b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer)
															&& "VulkanShaderBuffer only supports uniform and storage buffers!");
		layoutBuilder.AddBinding(b.binding, b.type);
		maxBinding = std::max(maxBinding, b.binding);
	}

	bindingCount = maxBinding + 1;
	buffers.resize(MAX_FRAME_OVERLAP * bindingCount);
	layout = layoutBuilder.Build(device->device, desc.stageFlags);

	for (u32 f = 0; f < MAX_FRAME_OVERLAP; ++f)
	{
		for (const auto& [binding, type, size] : desc.bindings)
		{
			const u32 i = index(f, binding);
			buffers[i] = VulkanBuffer(device, ToPreset(type), size);
		}
	}
}

void VulkanShaderBuffer::Destroy()
{
	for (auto& frameBuffers : buffers)
	{
		frameBuffers.Destroy();
	}

	buffers.clear();
	descriptorSets.reset();

	if (layout != nullptr)
	{
		vkDestroyDescriptorSetLayout(device->device, layout, nullptr);
	}
}

void VulkanShaderBuffer::UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) const
{
	ZoneScopedN("VulkanShaderBuffer::UpdateBinding");
	if (frameIndex >= buffers.size()) {
		LOG(Error, "ShaderBuffer: frameIndex {} out of range {}", frameIndex, buffers.size());
		return;
	}
	if (binding >= bindingCount) {
		LOG(Error, "ShaderBuffer: binding {} out of range {}", binding, bindingCount);
		return;
	}

	const VulkanBuffer& buf = buffers[index(frameIndex, binding)];
	if (!buf.IsValid()) {
		LOG(Error, "ShaderBuffer at frame={} binding={} is not valid!", frameIndex, binding);
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

void VulkanShaderBuffer::AllocateDescriptorSets()
{
	ZoneScoped;
	DescriptorWriter writer;
	writer.writes.reserve(desc.bindings.size());

	for (u32 frame = 0; frame < MAX_FRAME_OVERLAP; frame++)
	{
		ZoneScopedN("Allocating descriptor sets");
		DescriptorSet set = allocator->Allocate(DescriptorLayout{layout});
		if (set.vk == VK_NULL_HANDLE) {
			LOG(Error, "Failed to allocate descriptor set for frame {}", frame);
			continue;
		}
		descriptorSets[frame] = set;

		writer.Clear();

		for (const Binding& b : desc.bindings)
		{
			if (b.type == DescriptorType::UniformBuffer || b.type == DescriptorType::StorageBuffer) {
				const VulkanBuffer& buf = buffers[index(frame, b.binding)];
				if (!buf.IsValid()) {
					LOG(Error, "Invalid buffer at frame={} binding={}", frame, b.binding);
					continue;
				}
				writer.WriteBuffer(b.binding, &buf, b.type);
			}
		}

		writer.UpdateSet(device->device, set.vk);
	}
}

void VulkanShaderBuffer::Bind(VkCommandBuffer cmd, const VulkanPipeline& pipeline, u32 frameIndex, u32 setIndex) const
{
	ZoneScopedN("VulkanShaderBuffer::Bind");
	assert(frameIndex < MAX_FRAME_OVERLAP && "frameIndex out of range");
	const DescriptorSet set = descriptorSets[frameIndex];
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vkLayout, setIndex, 1, &set.vk, 0, nullptr);
}