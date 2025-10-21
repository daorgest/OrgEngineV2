//
// Created by Orgest on 6/12/2025.
//

#pragma once
#include <volk.h>
#include <tracy/TracyVulkan.hpp>

#include "RendererTypes.h"
#include "VulkanDescriptors.h"
#include "VulkanQueryPool.h"
#include "Tools/Vector.h"
namespace Renderer
{
	struct VulkanSwapchain;
	struct VulkanDevice;
	struct DescriptorAllocatorGrowable;
	struct VulkanCommands
	{
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		TracyVkCtx tracyCtx = nullptr;

		void Init(const VulkanDevice* device);
		void Destroy(const VulkanDevice* device);
		void Begin() const;
		void End() const;
	};

	struct VulkanFrameData
	{
		VulkanCommands command;
		VkFence renderFence = VK_NULL_HANDLE;
		VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
		VulkanQueryPool queryPool;
		DescriptorAllocatorGrowable descriptors;

		bool Init(VulkanDevice* device, TracyVkCtx tracyCtx = nullptr);
		void Destroy(const VulkanDevice* device);
	};

	struct FrameContext
	{
		VulkanCommands* commandContext = nullptr;
		VulkanFrameData* frameData = nullptr;
		u32 frameIndex = 0;
		u32 imageIndex = 0;
	};

	struct VulkanRenderer
	{
		Vector<VulkanFrameData> frames;
		Vector<VkSemaphore> presentSemaphores; // One per swapchain image (indexed by imageIndex)
		u32 frameNumber = 0;
		u32 framesActive = MAX_FRAME_OVERLAP;

		VulkanDevice* device = nullptr;
		VulkanSwapchain* swapchain = nullptr;
		TracyVkCtx tracyCtx = nullptr;

		bool Init(VulkanDevice* device, VulkanSwapchain* swapchain);
		void Destroy();
		[[nodiscard]] bool ResizeIfNeeded() const;
		FrameContext BeginFrame();
		void EndFrame(const FrameContext& frame);
		static void SetViewportAndScissor(VkCommandBuffer cmd, const Extent2D& extent);

		VulkanFrameData& GetCurrentFrame() { return frames[frameNumber % framesActive]; }
	};

}
