//
// Created by Orgest on 7/4/2025.
//

#include "VulkanShaderBuffer.h"

#include <ranges>

#include "Logger.h"
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanInit.h"

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
		layoutBuilder.AddBinding(b.binding, b.type);
		assert(b.type == DescriptorType::UniformBuffer || static_cast<bool>(DescriptorType::StorageBuffer) &&
			"VulkanShaderBuffer only supports uniform and storage buffers!");
		maxBinding = std::max(maxBinding, b.binding);
	}

	layout = layoutBuilder.Build(device->device, desc.stageFlags);

	buffers.resize(desc.framesInFlight);
	for (auto& bufPerFrame : buffers)
		bufPerFrame.resize(maxBinding + 1); // preallocate all binding slots

	for (u32 frame = 0; frame < desc.framesInFlight; frame++)
	{
		for (const Binding& b : desc.bindings)
		{
			if (b.type == DescriptorType::UniformBuffer)
				buffers[frame][b.binding] = VulkanBuffer(device, BufferPreset::UniformHost, b.size);
			if (b.type == DescriptorType::StorageBuffer)
				buffers[frame][b.binding] = VulkanBuffer(device, BufferPreset::StorageHostPersistent, b.size);
		}
	}

	descriptorSets.reserve(desc.framesInFlight);
}

void VulkanShaderBuffer::Destroy()
{
	for (auto& frameBuffers : buffers)
	{
		for (auto& buf : frameBuffers)
			buf.Destroy();
	}

	buffers.clear();

	descriptorSets.clear();

	vkDestroyDescriptorSetLayout(device->device, layout, nullptr);
	layout = VK_NULL_HANDLE;
}

void VulkanShaderBuffer::UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) const
{
	if (frameIndex >= buffers.size()) {
		LOG(Error, "ShaderBuffer: frameIndex {} out of range {}", frameIndex, buffers.size());
		return;
	}
	if (binding >= buffers[frameIndex].size()) {
		LOG(Error, "ShaderBuffer: binding {} out of range {}", binding, buffers[frameIndex].size());
		return;
	}

	const auto& buf = buffers[frameIndex][binding];
	if (!buf.buffer) {
		LOG(Error, "ShaderBuffer at frame={} binding={} is not valid!", frameIndex, binding);
		return;
	}
	buf.Upload(data, size);
}

void VulkanShaderBuffer::Update(uint32_t frameIndex, const void* data, size_t size) const
{
	UpdateBinding(frameIndex, 0, data, size);
}

void VulkanShaderBuffer::AllocateDescriptorSets()
{
	const u32 frameCount = static_cast<u32>(buffers.size());
	descriptorSets.resize(frameCount);

	VkDescriptorWriter writer;

	for (u32 frame = 0; frame < frameCount; frame++)
	{
		VkDescriptorSet set = allocator->Allocate(layout);
		descriptorSets[frame] = set;

		writer.Clear();

		for (const Binding& b : desc.bindings)
		{
			if (b.type == DescriptorType::UniformBuffer ||
				b.type == DescriptorType::StorageBuffer)
			{
				writer.WriteBuffer(b.binding, &buffers[frame][b.binding], b.type);
			}
		}

		writer.UpdateSet(device->device, set);
	}


}

void VulkanShaderBuffer::Bind(VkCommandBuffer cmd, VkPipelineLayout bindedLayout, u32 frameIndex, u32 setIndex) const
{
	assert(!descriptorSets.empty() && "descriptorSets not allocated! Did you call AllocateDescriptorSets()?");
	assert(frameIndex < descriptorSets.size());
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindedLayout, setIndex, 1, &descriptorSets[frameIndex], 0, nullptr);
}


void* VulkanShaderBuffer::GetNativeHandle(uint32_t frameIndex) const
{
	return descriptorSets[frameIndex];
}