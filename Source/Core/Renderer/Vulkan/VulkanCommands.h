//
// Created by Orgest on 6/12/2025.
//

#pragma once
#include <volk.h>
#include <tracy/TracyVulkan.hpp>

#include "VulkanCommandBuffer.h"
#include "VulkanQueryPool.h"
#include "Tools/Array.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct VulkanSwapchain;
	struct VulkanDevice;
	struct DescriptorAllocatorGrowable;

	/// Per-frame rendering data
	struct VulkanFrameData final : GPUFrameData
	{
		VulkanCommandBuffer commandBuffer;
		std::unique_ptr<VulkanFence> renderFence;
		std::unique_ptr<VulkanSemaphore> acquireSemaphore;
		VulkanQueryPool queryPool;

		// RHI interface implementation
		bool Init(GPUDevice* device) override;
		void Destroy() override;
		void Reset() override;

	    [[nodiscard]] VulkanCommandBuffer* GetCommandBuffer() override { return &commandBuffer; }

	private:
		VulkanDevice* device = nullptr;
	};

	// High-level renderer managing frame resources and presentation
	struct VulkanRenderer final : GPURenderer
	{
		bool Init(GPUDevice* device, GPUSwapchain* swapchain, u32 frameOverlap = MAX_FRAME_OVERLAP) override;
		bool BeginFrame(u32& outFrameIndex, u32& outImageIndex) override;
		void EndFrame(u32 frameIndex, u32 imageIndex) override;
		void Destroy() override;

		VulkanFrameData* GetCurrentFrameData() override { return &frames[frameNumber % framesActive]; }
		u32 GetFrameIndex() const override { return frameNumber % framesActive; }

		TracyVkCtx tracyCtx = nullptr;
	private:
		Array<VulkanFrameData, MAX_FRAME_OVERLAP> frames;
		Vector<std::unique_ptr<VulkanSemaphore>> presentSemaphores; // One per swapchain image (indexed by imageIndex)

		u32 frameNumber = 0;
		u32 framesActive = MAX_FRAME_OVERLAP;

		VulkanDevice* device = nullptr;
		VulkanSwapchain* swapchain = nullptr;
	};

} // namespace Renderer
