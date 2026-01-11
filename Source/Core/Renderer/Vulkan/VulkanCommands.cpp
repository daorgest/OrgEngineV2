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

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device->graphicsQueueIndex
    };

    VK_CHECK(vkCreateCommandPool(device->device, &poolInfo, nullptr, &commandPool));

    // Allocate primary command buffer directly
    commandBuffer.InitInternal(device, commandPool, false);

    queryPool.Init(device, 2);
    renderFence = std::make_unique<VulkanFence>(device);
    acquireSemaphore = std::make_unique<VulkanSemaphore>(device);

    return true;
}

void VulkanFrameData::Destroy()
{
    if (device)
    {
        commandBuffer.DestroyInternal();
        vkDestroyCommandPool(device->device, commandPool, nullptr);

        queryPool.Destroy();
        renderFence.reset();
        acquireSemaphore.reset();
    }
}

void VulkanFrameData::Reset()
{
    if (device)
    {
        vkResetCommandPool(device->device, commandPool, 0);
    }
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
                              frames.at(0).commandBuffer.GetVkHandle());
    TracyVkContextName(tracyCtx, "Graphics", 8);

    // Distribute Tracy context to all frame command buffers
    for (u32 i = 0; i < framesActive; i++)
    {
        frames[i].commandBuffer.tracyCtx = tracyCtx;
    }

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
    auto* vkFrame = &frames[outFrameIndex];

    // Wait for this frame's fence
    vkWaitForFences(device->device, 1, &vkFrame->renderFence->fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device->device, 1, &vkFrame->renderFence->fence);

    vkFrame->Reset();

    auto acquireResult = swapchain->AcquireNextImage(vkFrame->acquireSemaphore.get());
    if (!acquireResult.has_value()) return false;

    outImageIndex = acquireResult.value();

    vkFrame->commandBuffer.Begin(nullptr);

    // The GPU is done, collect timestamps!!
    if (tracyCtx)
    {
        TracyVkCollect(tracyCtx, vkFrame->commandBuffer.GetVkHandle());
    }
    return true;
}

void VulkanRenderer::EndFrame(u32 frameIndex, u32 imageIndex)
{
    VulkanFrameData& frame = frames[frameIndex];

    VkSemaphore sem = presentSemaphores[imageIndex]->Get();
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

    VK_CHECK(vkQueuePresentKHR(device->graphicsQueue, &presentInfo));
    frameNumber++;
}
