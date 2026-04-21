//
// Created by Orgest on 6/12/2025.
//

#include "VulkanCommands.h"

#include "Platform.h"
#include "VulkanCheck.h"
#include "VulkanSwapchain.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"
#include "Tools/Logger.h"

using namespace Renderer;

// FrameData
bool VulkanFrameData::Init(GPUDevice* dev)
{
    device = static_cast<VulkanDevice*>(dev);

    // Allocate primary command buffer and pool directly
    commandBuffer.Init(device);

    queryPool.Init(device, 4);
    renderFence = std::make_unique<VulkanFence>(device);
    acquireSemaphore = std::make_unique<VulkanSemaphore>(device);

    return true;
}

void VulkanFrameData::Destroy()
{
    if (device)
    {
        commandBuffer.Destroy();
        queryPool.Destroy();
    }
}

void VulkanFrameData::Reset()
{
    // if (device)
    // {
    //     vkResetCommandPool(device->device, commandBuffer.GetVkPool(), 0);
    // }
}

// VulkanRenderer
bool VulkanRenderer::Init(GPUDevice* dev, GPUSwapchain* sc, u32 frameOverlap)
{
    device = static_cast<VulkanDevice*>(dev);
    swapchain = static_cast<VulkanSwapchain*>(sc);
    framesActive = std::min(frameOverlap, static_cast<u32>(MAX_FRAME_OVERLAP));

    for (auto& frame : frames)
    {
        frame.Init(device);
    }

    // Initialize Tracy profiling context ONCE
    tracyCtx = TracyVkContext(device->physicalDevice, device->device, device->graphicsQueue,
                              frames[0].commandBuffer.GetVkHandle());
    TracyVkContextName(tracyCtx, "Graphics", 8);

    // Distribute Tracy context to all frame command buffers
    for (u32 i = 0; i < framesActive; i++)
    {
        frames[i].commandBuffer.tracyCtx = tracyCtx;
    }
    device->immediateSubmitter.cmdBuffer.tracyCtx = tracyCtx;

    // Allocate present semaphores (matching swapchain image count)
    for (u32 i = 0; i < swapchain->imageCount; i++)
    {
        presentSemaphores.push_back(std::make_unique<VulkanSemaphore>(device));
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
        tracyCtx = nullptr;
    }

    for (auto& frame : frames)
    {
        frame.Destroy();
    }

    presentSemaphores.clear();
}

bool VulkanRenderer::BeginFrame(u32& outFrameIndex, u32& outImageIndex)
{
    outFrameIndex = frameNumber % framesActive;

    VulkanFrameData& frame = frames[outFrameIndex];

    frame.commandBuffer.WaitForFence(frame.renderFence.get());
    // Only fetch results if this frame slot has been written to before!
    // If we are on Frame 0 or 1 (in a double-buffered system), this memory is fresh
    // and reading it causes the validation error.
    if (frameNumber >= framesActive)
    {
        frame.queryPool.FetchResults();
    }

    auto acquireResult = swapchain->AcquireNextImage(frame.acquireSemaphore.get(), outImageIndex);
    if (!acquireResult.has_value())
    {
        if (acquireResult.error() == VulkanSwapchainOutOfDate || Suboptimal)
        {
            swapchain->needsRecreation = true;
        }
        return false;
    }


    outImageIndex = acquireResult.value();

    frame.commandBuffer.Begin(nullptr);
    frame.commandBuffer.BeginDebugLabel("Frame CommandBuffer", 0.0f, 0.7f, 1.0f);

    frame.queryPool.Reset(&frame.commandBuffer);

    // The GPU is done, collect timestamps!!
    if (tracyCtx)
    {
        TracyVkCollect(tracyCtx, frame.commandBuffer.GetVkHandle());
    }
    return true;
}

void VulkanRenderer::EndFrame(u32 frameIndex, u32 imageIndex)
{
    VulkanFrameData& frame = frames[frameIndex];

    VkSemaphore sem = presentSemaphores[imageIndex]->semaphore;
	frame.commandBuffer.EndDebugLabel();
    frame.commandBuffer.End();

    // Sync2 Submission!
    VkCommandBufferSubmitInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = frame.commandBuffer.GetVkHandle(),
        .deviceMask = 0
    };

    VkSemaphoreSubmitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frame.acquireSemaphore->semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT // Wait for image acquisition
    };

    VkSemaphoreSubmitInfo signalInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = sem,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT // Signal completion to presenter
    };

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo
    };


    VK_CHECK(vkQueueSubmit2(device->graphicsQueue, 1, &submitInfo, frame.renderFence->fence));

    // Presentation
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->swapchain,
        .pImageIndices = &imageIndex
    };

    const VkResult result = vkQueuePresentKHR(device->graphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        swapchain->needsRecreation = true;
    }
    frameNumber++;
}
