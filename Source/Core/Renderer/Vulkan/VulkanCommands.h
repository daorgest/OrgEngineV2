//
// Created by Orgest on 6/12/2025.
//

#pragma once
#include <volk.h>
#ifdef TRACY_ENABLE
#include <tracy/TracyVulkan.hpp>
#endif
#include "VulkanCommandBuffer.h"
#include "VulkanQueryPool.h"
#include "Tools/Array.h"

namespace Renderer
{
	struct VulkanSwapchain;
	struct VulkanDevice;
	struct DescriptorAllocatorGrowable;

	/// Per-frame rendering data
	struct VulkanFrameData final : GPUFrameData
	{
		VulkanCommandBuffer commandBuffer;
	    VulkanQueryPool queryPool;

	    VulkanFence renderFence;
	    VulkanSemaphore acquireSemaphore;

		bool Init(GPUDevice* device) override;
		void Destroy() override;

	    [[nodiscard]] VulkanCommandBuffer* GetCommandBuffer() override { return &commandBuffer; }
	    [[nodiscard]] VulkanQueryPool* GetQueryPool() override { return &queryPool; }

	private:
		VulkanDevice* device = nullptr;
	};

	// High-level renderer managing frame resources and presentation
    struct VulkanRenderer final : GPURenderer
    {
        bool Init(GPUDevice* device, GPUSwapchain* swapchain, u32 frameOverlap = MAX_FRAME_OVERLAP) override;
        void Destroy() override;

        [[nodiscard]] GPUCommandBuffer* BeginFrame() override;
        void EndFrame() override;

        [[nodiscard]] GPUFrameData* GetCurrentFrameData() override { return &frames[frameNumber % framesActive]; }
        [[nodiscard]] u32 GetFrameIndex() const override { return frameNumber % framesActive; }
    private:
        Array<VulkanFrameData, MAX_FRAME_OVERLAP> frames;
        Array<VulkanSemaphore, MAX_FRAME_OVERLAP> presentSemaphores;
        u32 currentImageIndex = 0;
        u32 frameNumber = 0;
        u32 framesActive = MAX_FRAME_OVERLAP;
#ifdef TRACY_ENABLE
        TracyVkCtx tracyCtx = nullptr;
#endif
        VulkanDevice* device = nullptr;
        VulkanSwapchain* swapchain = nullptr;
    };

} // namespace Renderer
