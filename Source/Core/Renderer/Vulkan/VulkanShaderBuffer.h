//
// Created by Orgest on 7/4/2025.
//

#pragma once
#include <volk.h>

#include "RendererTypes.h"
#include "VulkanDescriptors.h"
#include "VulkanBuffer.h"
#include "Tools/Array.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct GPUCommandBuffer;
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
		DescriptorSetLayoutDesc desc;
		Vector<u32> bindingToSlot;       // binding -> slot index
		u32 slotCount = 0;               // number of actual UBOs/SSBOs

		[[nodiscard]] auto index(const u32 frame, const u32 binding) const noexcept -> u32
		{
			const u32 slot = bindingToSlot[binding];
			return (frame * slotCount) + slot;
		}

		VulkanShaderBuffer(VulkanDevice* dev, DescriptorAllocatorGrowable* alloc, const DescriptorSetLayoutDesc& desc);

		// Raw update with manual size
		void UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) const;

		void Update(u32 frameIndex, const void* data, size_t size) const;
		void Bind(GPUCommandBuffer* cmd, const VulkanPipeline& pipeline, u32 frameIndex) const;
		void AllocateDescriptorSets(bool isBindless = false, u32 bindlessCount = 1);
		void Destroy();
	};
}
