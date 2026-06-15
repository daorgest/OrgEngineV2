//
// Created by Orgest on 6/12/2025.
//

#include "VulkanCommands.h"

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

    queryPool.Init(device, 4); // Scene -> UI timings (Incuding that compute popup)
    commandBuffer.Init(device);
    renderFence.Init(device);
    acquireSemaphore.Init(device);

    return true;
}

void VulkanFrameData::Destroy()
{
    renderFence.Destroy();
    acquireSemaphore.Destroy();
    commandBuffer.Destroy();
    queryPool.Destroy();
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

    for (auto& semaphore : presentSemaphores)
    {
        semaphore.Init(device);
    }

#ifdef TRACY_ENABLE
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device->graphicsQueueIndex
    };
    VkCommandPool tracyPool;
    vkCreateCommandPool(device->device, &poolInfo, nullptr, &tracyPool);

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = tracyPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer tracySetupCmd;
    vkAllocateCommandBuffers(device->device, &allocInfo, &tracySetupCmd);

    tracyCtx = TracyVkContext(device->physicalDevice, device->device, device->graphicsQueue, tracySetupCmd);
    TracyVkContextName(tracyCtx, "Graphics", 8);

    vkDestroyCommandPool(device->device, tracyPool, nullptr);

    for (u32 i = 0; i < framesActive; i++)
    {
        frames[i].commandBuffer.tracyCtx = tracyCtx;
    }
    device->immediateSubmitter.cmdBuffer.tracyCtx = tracyCtx;
#endif

    frameNumber = 0;
    return true;
}

void VulkanRenderer::Destroy()
{
    if (!device || !device->device) return;

    device->WaitIdle();

    if (tracyCtx)
    {
        TracyVkDestroy(tracyCtx);
        tracyCtx = nullptr;
    }

    for (auto& frame : frames)
    {
        frame.Destroy();
    }

    for (auto& semaphore : presentSemaphores)
    {
        semaphore.Destroy();
    }
}

GPUCommandBuffer* VulkanRenderer::BeginFrame()
{
    const u32 currentFrameIndex = frameNumber % framesActive;
    VulkanFrameData& frame = frames[currentFrameIndex];
    frame.commandBuffer.WaitForFence(&frame.renderFence);

    // Only fetch results if this frame slot has been written to before!
    // If we are on Frame 0 or 1 (in a double-buffered system), this memory is fresh
    // and reading it causes the validation error.
    if (frameNumber >= framesActive)
    {
        frame.queryPool.FetchResults();
    }
    auto acquireResult = swapchain->AcquireNextImage(&frame.acquireSemaphore);
    if (!acquireResult.has_value())
    {
        swapchain->needsRecreation = true;
        return nullptr;
    }
    currentImageIndex = acquireResult.value();

    frame.commandBuffer.Begin(nullptr);
    frame.commandBuffer.BeginDebugLabel("Frame CommandBuffer", 0.0f, 0.7f, 1.0f);
    frame.queryPool.Reset(&frame.commandBuffer);

#ifdef TRACY_ENABLE
    if (tracyCtx)
    {
        TracyVkCollect(tracyCtx, frame.commandBuffer.GetVkHandle());
    }
#endif
    return &frame.commandBuffer;
}

void VulkanRenderer::EndFrame()
{
    if (!swapchain) return;

    const u32 currentFrameIndex = frameNumber % framesActive;
    VulkanFrameData& frame = frames[currentFrameIndex];

    frame.commandBuffer.EndDebugLabel();
    frame.commandBuffer.End();

    const VkSemaphore renderFinishedSem = presentSemaphores[currentImageIndex].semaphore;

    // Sync2 Submission!
    VkCommandBufferSubmitInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = frame.commandBuffer.GetVkHandle(),
        .deviceMask = 0
    };

    VkSemaphoreSubmitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frame.acquireSemaphore.semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT // Wait for image acquisition
    };

    VkSemaphoreSubmitInfo signalInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderFinishedSem,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
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

    VK_CHECK(vkQueueSubmit2(device->graphicsQueue, 1, &submitInfo, frame.renderFence.fence));

    // Presentation
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSem,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->swapchain,
        .pImageIndices = &currentImageIndex
    };

    const VkResult result = vkQueuePresentKHR(device->graphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        swapchain->needsRecreation = true;
    }
    frameNumber++;
}
