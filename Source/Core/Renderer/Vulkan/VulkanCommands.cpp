//
// Created by Orgest on 6/12/2025.
//

#include "VulkanCommands.h"

#include <set>

#include "DeletionQueue.h"
#include "Logger.h"
#include "Platform.h"
#include "VulkanCheck.h"
#include "VulkanInit.h"
#include "VulkanSwapchain.h"

using namespace Renderer;

void VulkanCommands::Init(const VulkanDevice* device)
{
	VkCommandPoolCreateInfo commandPoolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = device->graphicsQueueIndex
	};

	VK_CHECK(vkCreateCommandPool(device->device, &commandPoolInfo, nullptr, &commandPool));

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VK_CHECK(vkAllocateCommandBuffers(device->device, &commandBufferAllocateInfo, &commandBuffer));
}

void VulkanCommands::Destroy(const VulkanDevice* device)
{
	if (commandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device->device, commandPool, nullptr);
		commandPool = VK_NULL_HANDLE;
		commandBuffer = VK_NULL_HANDLE;
	}
}

void VulkanCommands::Begin() const
{
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	CmdBeginLabel(commandBuffer, "Frame CommandBuffer", 0.0f, 0.7f, 1.0f);
}

void VulkanCommands::End() const
{
	CmdEndLabel(commandBuffer);
	vkEndCommandBuffer(commandBuffer);
}

bool VulkanFrameData::Init(VulkanDevice* device, TracyVkCtx tracyCtx)
{
	command.Init(device);
	command.tracyCtx = tracyCtx;


	queryPool.Init(device, 2);

	// init descriptor allocs
	Vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3  },
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
	};

	descriptors.Init(device, 1000, frameSizes);

	VkFenceCreateInfo fenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &renderFence));
	VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VK_CHECK(vkCreateSemaphore(device->device, &semInfo, nullptr, &acquireSemaphore));

	return true;
}

void VulkanFrameData::Destroy(const VulkanDevice* device)
{
	descriptors.DestroyPools();

	vkDestroyFence(device->device, renderFence, nullptr);
	vkDestroySemaphore(device->device, acquireSemaphore, nullptr);

	command.Destroy(device);
	queryPool.Destroy();
}

bool VulkanRenderer::Init(VulkanDevice* dev, VulkanSwapchain* sc)
{
	device = dev;
	swapchain = sc;

	frames.resize(MAX_FRAME_OVERLAP);
	frames.reserve(MAX_FRAME_OVERLAP);
	for (auto& frame : frames)
	{
		frame.Init(device);
	}

	tracyCtx = TracyVkContext(device->physicalDevice, device->device, device->graphicsQueue, frames[0].command.commandBuffer);

	for (auto& frame : frames)
	{
		frame.command.tracyCtx = tracyCtx;
	}

	presentWaitSemaphores.resize(swapchain->imageCount);
	constexpr VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	for (auto& sem : presentWaitSemaphores)
	{
		VK_CHECK(vkCreateSemaphore(device->device, &semInfo, nullptr, &sem));
	}

	frameNumber  = 0;
	return true;
}

void VulkanRenderer::Destroy()
{
	vkDeviceWaitIdle(device->device);

	if (tracyCtx != nullptr)
		TracyVkDestroy(tracyCtx);

	for (auto& frame : frames)
	{
		frame.Destroy(device);
	}
	for (auto& sem : presentWaitSemaphores)
	{
		if (sem != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device->device, sem, nullptr);
			sem = VK_NULL_HANDLE;
		}
	}
	presentWaitSemaphores.clear();
}

bool VulkanRenderer::ResizeIfNeeded() const
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
		return true;
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


FrameContext VulkanRenderer::BeginFrame()
{
	VulkanFrameData& frame = GetCurrentFrame();

	vkWaitForFences(device->device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);
	vkResetFences(device->device, 1, &frame.renderFence);

	// reset descriptors for this frame
	frame.descriptors.ResetPools();

	u32 imageIndex = 0;
	VkResult res = vkAcquireNextImageKHR(device->device, swapchain->swapchain, UINT64_MAX, frame.acquireSemaphore, VK_NULL_HANDLE,
	                                     &imageIndex);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		swapchain->Resize();
		return {};
	}

	frame.command.Begin();
	frame.queryPool.Reset(frame.command.commandBuffer);
	return FrameContext{
		.commandContext = &frame.command,
		.frameData = &frame,
		.frameIndex = frameNumber % framesActive,
		.imageIndex = imageIndex,
	};
}

void VulkanRenderer::SetViewportAndScissor(VkCommandBuffer cmd, const Extent2D& extent)
{
	VkViewport viewport = {0.f, 0.f, (f32)extent.width, (f32)extent.height, 1.f, 0.f};
	VkRect2D scissor = {{0, 0}, {extent.width, extent.height}};

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanRenderer::EndFrame(const FrameContext& frame)
{
	u32 imageIndex = frame.imageIndex;

	frame.commandContext->End();

	VkCommandBuffer cmd = frame.commandContext->commandBuffer;


	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &frame.frameData->acquireSemaphore,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &presentWaitSemaphores[imageIndex]
	};

	VK_CHECK(vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, frame.frameData->renderFence));
	frame.frameData->queryPool.FetchResults();

	TracyVkCollect(frame.commandContext->tracyCtx , cmd);

	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &presentWaitSemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &swapchain->swapchain,
		.pImageIndices = &imageIndex
	};

	VK_CHECK(vkQueuePresentKHR(device->graphicsQueue, &presentInfo));
	frameNumber = (frameNumber + 1) % framesActive;
}