//
// Created by Orgest on 7/4/2025.
//

#pragma once
#include <volk.h>

#include "RendererTypes.h"
#include "VulkanDescriptors.h"
#include "Tools/Array.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct VulkanBuffer;
	struct VulkanSampler;
	struct VulkanImage;
	struct VulkanDevice;
	struct VulkanPipeline;
	struct VulkanShaderBuffer
	{
		VulkanDevice* device = nullptr;
		DescriptorAllocatorGrowable* allocator = nullptr;
		Vector<VulkanBuffer> buffers;
		Array<DescriptorSet, MAX_FRAME_OVERLAP> descriptorSets;
		DescriptorLayout layout;
		UniformBufferDesc desc;
		u32 bindingCount = 0;

		[[nodiscard]] u32 index(u32 frame, u32 binding) const { return (frame * bindingCount) + binding; }

		VulkanShaderBuffer(VulkanDevice* dev,  DescriptorAllocatorGrowable* alloc, const UniformBufferDesc& desc);
		void UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) const;

		void Update(u32 frameIndex, const void* data, size_t size) const;
		void Bind(VkCommandBuffer cmd, const VulkanPipeline& pipeline, u32 frameIndex, u32 setIndex) const;
		void AllocateDescriptorSets();
		void Destroy();
	};
}
