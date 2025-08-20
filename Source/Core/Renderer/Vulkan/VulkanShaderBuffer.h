//
// Created by Orgest on 7/4/2025.
//

#pragma once

#include "Vector.h"
#include "VulkanBuffer.h"

namespace Renderer
{
	struct VulkanSampler;
	struct VulkanImage;
	struct DescriptorAllocatorGrowable;
	struct VulkanDevice;
	struct VulkanShaderBuffer
	{
		VulkanDevice* device = nullptr;
		DescriptorAllocatorGrowable* allocator = nullptr;
		VkDescriptorSetLayout layout = VK_NULL_HANDLE;

		Vector<Vector<VulkanBuffer>> buffers;

		Vector<VkDescriptorSet> descriptorSets;

		UniformBufferDesc desc;

		VulkanShaderBuffer(VulkanDevice* dev,  DescriptorAllocatorGrowable* alloc, const UniformBufferDesc& desc);
		void Destroy();
		void UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) const;

		void Update(u32 frameIndex, const void* data, size_t size) const;
		void Bind(VkCommandBuffer cmd, VkPipelineLayout bindedLayout, u32 frameIndex, u32 setIndex) const;
		[[nodiscard]] void* GetNativeHandle(uint32_t frameIndex) const;

		void AllocateDescriptorSets();
	};
}
