//
// Created by Orgest on 7/4/2025.
//

#pragma once

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
	struct VulkanDevice;
	struct VulkanPipeline;
	struct VulkanShaderBuffer final : GPUShaderBuffer
	{
		VulkanDevice* device = nullptr;
		DescriptorAllocatorGrowable* allocator = nullptr;
		Vector<VulkanBuffer> buffers;
		Array<VulkanDescriptorSet, MAX_FRAME_OVERLAP> descriptorSets;
		DescriptorLayout layout;
		DescriptorSetLayoutDesc desc;
		Vector<u32> bindingToSlot;       // binding -> slot index
		u32 slotCount = 0;               // number of actual UBOs/SSBOs

	    VulkanShaderBuffer(GPUDevice* device, DescriptorAllocatorGrowable* alloc, const DescriptorSetLayoutDesc& desc);
	    ~VulkanShaderBuffer() override { Destroy(); }

	    void UpdateBinding(u32 frameIndex, u32 binding, const void* data, size_t size) override;
        void Update(u32 frameIndex, const void* data, size_t size);
        void Bind(GPUCommandBuffer* cmd, GPUPipeline* pipeline, u32 frameIndex) override;
	    void Destroy() override;

	    // Internal setup helpers
	    void Initialize();
	    void AllocateDescriptorSets(bool isBindless = false, u32 bindlessCount = 1);
	private:
	    [[nodiscard]] u32 index(u32 frame, u32 binding) const noexcept;
	};
}
