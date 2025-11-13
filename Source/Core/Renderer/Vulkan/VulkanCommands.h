//
// Created by Orgest on 6/12/2025.
//

#pragma once
#include <volk.h>
#include <tracy/TracyVulkan.hpp>

#include "RendererTypes.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanQueryPool.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct VulkanSwapchain;
	struct VulkanDevice;
	struct DescriptorAllocatorGrowable;

	/// Per-frame rendering data
	/// Contains command buffer, synchronization objects, and per-frame allocators
	struct VulkanFrameData final : GPUFrameData
	{
		VulkanCommandBuffer commandBuffer;
		VulkanCommandPool commandPool;
		VkFence renderFence = VK_NULL_HANDLE;
		VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
		VulkanQueryPool queryPool;
		DescriptorAllocatorGrowable descriptors;
		TracyVkCtx tracyCtx = nullptr;

		// RHI interface implementation
		bool Init(GPUDevice* device) override;
		void Destroy() override;
		void Reset() override;

		// Vulkan-specific Init (with Tracy context)
		bool Init(VulkanDevice* device, TracyVkCtx tracyCtx = nullptr);

		GPUCommandBuffer* GetCommandBuffer() override { return &commandBuffer; }
		void* GetRenderFence() override { return &renderFence; }
		void* GetAcquireSemaphore() override { return &acquireSemaphore; }
		void* GetPresentSemaphore() override { return nullptr; } // Managed by renderer

	private:
		VulkanDevice* device = nullptr;
	};

	/// Frame context returned by BeginFrame
	struct FrameContext
	{
		GPUCommandBuffer* commandBuffer = nullptr;
		VulkanFrameData* frameData = nullptr;
		u32 frameIndex = 0;
		u32 imageIndex = 0;

		// Backward compatibility alias
		GPUCommandBuffer*& commandContext = commandBuffer;
	};

	/// High-level renderer managing frame resources and presentation
	/// Marked final to enable compiler devirtualization
	struct VulkanRenderer final : GPURenderer
	{
		bool Init(GPUDevice* device, GPUSwapchain* swapchain, u32 frameOverlap = MAX_FRAME_OVERLAP) override;
		void Destroy() override;

		bool BeginFrame(u32& frameIndex, u32& imageIndex) override;
		void EndFrame(u32 frameIndex, u32 imageIndex) override;
		bool ResizeIfNeeded() override;

		GPUFrameData* GetCurrentFrameData() override { return &frames[frameNumber % framesActive]; }
		[[nodiscard]] u32 GetFrameNumber() const override { return frameNumber; }

		// Compatibility methods for old FrameContext API (will be deprecated)
		FrameContext BeginFrame();
		void EndFrame(const FrameContext& frame);

		// Vulkan-specific helpers
		VulkanFrameData& GetCurrentFrame() { return frames[frameNumber % framesActive]; }
		static void SetViewportAndScissor(VkCommandBuffer cmd, const Extent2D& extent);

	private:
		Vector<VulkanFrameData> frames;
		Vector<VkSemaphore> presentSemaphores; // One per swapchain image (indexed by imageIndex)
		u32 frameNumber = 0;
		u32 framesActive = MAX_FRAME_OVERLAP;

		VulkanDevice* device = nullptr;
		VulkanSwapchain* swapchain = nullptr;
		TracyVkCtx tracyCtx = nullptr;
	};

} // namespace Renderer
