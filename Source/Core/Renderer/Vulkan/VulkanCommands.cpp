//
// Created by Orgest on 6/12/2025.
// Updated by Orgest on 11/4/2025 - Refactored to use RHI abstraction
//

#include "VulkanCommands.h"

#include "Platform.h"
#include "VulkanCheck.h"
#include "VulkanInit.h"
#include "VulkanSwapchain.h"
#include "VulkanCommandBuffer.h"
#include "Tools/Logger.h"

using namespace Renderer;

// VulkanFrameData Implementation

bool VulkanFrameData::Init(GPUDevice* dev)
{
	return Init(static_cast<VulkanDevice*>(dev), nullptr);
}

bool VulkanFrameData::Init(VulkanDevice* dev, TracyVkCtx ctx)
{
	device = dev;
	tracyCtx = ctx;

	// Initialize command pool
	commandPool.Init(device, device->graphicsQueueIndex, false);

	// Allocate primary command buffer directly
	commandBuffer.InitInternal(device, commandPool.GetVkHandle(), false);
	commandBuffer.tracyCtx = tracyCtx;


	queryPool.Init(device, 4);

	// Init descriptor allocators
	Vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
		{DescriptorType::StorageImage, 3},
		{DescriptorType::StorageBuffer, 3},
		{DescriptorType::UniformBuffer, 3},
		{DescriptorType::Sampler, 4},
	};
	descriptors.Init(device, 1000, frameSizes);

	// Create synchronization objects
	VkFenceCreateInfo fenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &renderFence));

	VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VK_CHECK(vkCreateSemaphore(device->device, &semInfo, nullptr, &acquireSemaphore));

	return true;
}

void VulkanFrameData::Destroy()
{
	if (!device) return;

	descriptors.DestroyPools();

	if (renderFence != VK_NULL_HANDLE)
	{
		vkDestroyFence(device->device, renderFence, nullptr);
		renderFence = VK_NULL_HANDLE;
	}

	if (acquireSemaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(device->device, acquireSemaphore, nullptr);
		acquireSemaphore = VK_NULL_HANDLE;
	}

	commandBuffer.DestroyInternal();
	commandPool.Destroy();
	queryPool.Destroy();
}

void VulkanFrameData::Reset()
{
	descriptors.ResetPools();
	commandPool.Reset();
}

// VulkanRenderer Implementation

bool VulkanRenderer::Init(GPUDevice* dev, GPUSwapchain* sc, u32 frameOverlap)
{
	device = static_cast<VulkanDevice*>(dev);
	swapchain = reinterpret_cast<VulkanSwapchain*>(sc); // TODO: Make VulkanSwapchain inherit from GPUSwapchain
	framesActive = frameOverlap;

	frames.resize(framesActive);
	for (auto& frame : frames)
	{
		frame.Init(device);
	}

	// Initialize Tracy profiling context
	tracyCtx = TracyVkContext(device->physicalDevice, device->device, device->graphicsQueue,
	                          frames.at(0).commandBuffer.GetVkHandle());
	TracyVkContextName(tracyCtx, "Graphics", 8);

	for (auto& frame : frames)
	{
		frame.commandBuffer.tracyCtx = tracyCtx;
	}

	// Create present semaphores (one per swapchain image)
	presentSemaphores.resize(swapchain->imageCount);
	constexpr VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	for (auto& sem : presentSemaphores)
	{
		VK_CHECK(vkCreateSemaphore(device->device, &semInfo, nullptr, &sem));
	}

	frameNumber = 0;
	return true;
}

void VulkanRenderer::Destroy()
{
	vkDeviceWaitIdle(device->device);

	if (tracyCtx)
	{
		TracyVkDestroy(tracyCtx);
	}

	for (auto& frame : frames)
	{
		frame.Destroy();
	}

	for (auto& sem : presentSemaphores)
	{
		if (sem != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device->device, sem, nullptr);
			sem = VK_NULL_HANDLE;
		}
	}
	presentSemaphores.clear();
	frames.clear();
}

bool VulkanRenderer::ResizeIfNeeded()
{
	static bool wasMinimized = false;

	u32 newWidth = 0;
	u32 newHeight = 0;
	Platform::GetWindowSize(swapchain->handle, newWidth, newHeight);

	// Window minimized → skip rendering & resizing
	if (newWidth == 0 || newHeight == 0)
	{
		if (!wasMinimized)
		{
			LOG(Warning, "Window minimized, skipping resize");
			wasMinimized = true;
		}
		return true; // Skip frame
	}

	// Window restored
	if (wasMinimized)
	{
		LOG(Info, "Window restored.");
		wasMinimized = false;
	}

	// Check for actual resize
	if (newWidth != swapchain->width || newHeight != swapchain->height)
	{
		if (!swapchain->Resize())
		{
			LOG(Error, "Failed to resize swapchain.");
			return true;
		}

		return true; // Resized → skip current frame
	}

	return false; // No resize or minimize → proceed to render
}
bool VulkanRenderer::BeginFrame(u32& frameIndex, u32& imageIndex)
{
	VulkanFrameData& frame = GetCurrentFrame();

	// Wait for this frame's fence
	vkWaitForFences(device->device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);
	vkResetFences(device->device, 1, &frame.renderFence);

	// Reset per-frame resources
	frame.Reset();

	// Acquire next swapchain image
	imageIndex = 0;
	VkResult res = vkAcquireNextImageKHR(device->device, swapchain->swapchain, UINT64_MAX,
	                                     frame.acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		swapchain->Resize();
		return false; // Skip frame
	}

	// Begin command recording
	frame.commandBuffer.Begin(nullptr);
	frame.queryPool.Reset(frame.commandBuffer.GetVkHandle());

	frameIndex = frameNumber % framesActive;
	return true;
}

// Old API for compatibility - returns FrameContext
FrameContext VulkanRenderer::BeginFrame()
{
	u32 fIndex, iIndex;
	if (!BeginFrame(fIndex, iIndex)) {
		return {}; // Failed
}

	VulkanFrameData& frame = GetCurrentFrame();
	return FrameContext{
		.commandBuffer = &frame.commandBuffer,
		.frameData = &frame,
		.frameIndex = fIndex,
		.imageIndex = iIndex,
	};
}

void VulkanRenderer::EndFrame(u32 frameIndex, u32 imageIndex)
{
	VulkanFrameData& frame = frames[frameIndex];
	VkCommandBuffer cmd = frame.commandBuffer.GetVkHandle();

	// Collect Tracy profiling data
	TracyVkCollect(frame.tracyCtx, cmd);

	// End command recording
	frame.commandBuffer.End();

	// Submit to GPU
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &frame.acquireSemaphore,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &presentSemaphores[imageIndex]
	};

	VK_CHECK(vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, frame.renderFence));

	// Present to screen
	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &presentSemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &swapchain->swapchain,
		.pImageIndices = &imageIndex
	};

	VK_CHECK(vkQueuePresentKHR(device->graphicsQueue, &presentInfo));

	frameNumber++; // Increment frame counter
}

// Compatibility method for old API
void VulkanRenderer::EndFrame(const FrameContext& frame)
{
	EndFrame(frame.frameIndex, frame.imageIndex);
}

void VulkanRenderer::SetViewportAndScissor(VkCommandBuffer cmd, const Extent2D& extent)
{
	VkViewport viewport = {0.f, 0.f, static_cast<f32>(extent.width), static_cast<f32>(extent.height), 0.f, 1.f};
	VkRect2D scissor = {{0, 0}, {extent.width, extent.height}};

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

