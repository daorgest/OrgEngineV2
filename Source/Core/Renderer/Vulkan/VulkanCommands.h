//
// Created by Orgest on 6/12/2025.
//

#pragma once
#include <volk.h>

#include "RendererTypes.h"
#include "Vector.h"
#include "VulkanDescriptors.h"
#include "VulkanQueryPool.h"
#include "tracy/TracyVulkan.hpp"

static constexpr u32 MAX_FRAME_OVERLAP = 2;

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
		Vector<VkSemaphore> presentWaitSemaphores;
		u32 frameNumber = 0;
		u32 framesActive = 2; // double buffer by default

		VulkanDevice* device = nullptr;
		VulkanSwapchain* swapchain = nullptr;
		TracyVkCtx tracyCtx = nullptr;

		bool Init(VulkanDevice* device, VulkanSwapchain* swapchain);
		void Destroy();
		[[nodiscard]] bool ResizeIfNeeded() const;
		FrameContext BeginFrame();
		static void SetViewportAndScissor(VkCommandBuffer cmd, const Extent2D& extent);

		VulkanFrameData& GetCurrentFrame() { return frames[frameNumber % framesActive]; }

		// Transitions
		void EndFrame(const FrameContext& frame);
	};

}
