//
// Created by Orgest on 6/11/2025.
//

#include "VulkanSwapchain.h"

#define VOLK_IMPLEMENTATION
#include <windows.h>

#include "Platform.h"
#include "VulkanCheck.h"
#include "VulkanCommandBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDevice.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"
#if ENGINE_PLATFORM_SDL
#include "SDL3/SDL_vulkan.h"
#endif
#include "Tools/Logger.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

Result<void> VulkanSwapchain::Init(GPUDevice* device, WindowHandle windowHandle_)
{
    auto* devicePtr = static_cast<VulkanDevice*>(device);

    ZoneScopedN("Init Swapchain");
    this->vkDev = devicePtr;
    this->handle = windowHandle_;

    LOG(Info, "Initializing Vulkan swapchain...");

#if ENGINE_PLATFORM_WIN32
    const VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = static_cast<HWND>(handle)
    };
    VK_CHECK(vkCreateWin32SurfaceKHR(devicePtr->instance->instance, &surfaceCreateInfo, nullptr, &surface));
    LOG(Info, "Created Win32 Vulkan surface.");
#elif ENGINE_PLATFORM_SDL
    if (!SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(handle), devicePtr->instance->instance, nullptr, &surface))
    {
        LOG(Error, "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return std::unexpected(OrgErrCode::SurfaceLost);
    }
    LOG(Info, "Created SDL3 Vulkan surface.");
#else
#error "Unsupported platform for Vulkan surface creation"
#endif

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devicePtr->physicalDevice, surface, &caps);

    // the surface can sometimes bork
    if (caps.currentExtent.width == UNDEFINED_EXTENT)
    {
        Platform::GetWindowSize(handle, width, height);
    }

    width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);

    renderWidth = static_cast<u32>(width * renderScale);
    renderHeight = static_cast<u32>(height * renderScale);

    // Query supported surface formats
    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(devicePtr->physicalDevice, surface, &formatCount, nullptr);
    Vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(devicePtr->physicalDevice, surface, &formatCount, formats.data());

    surfaceFormat = PickSurfaceFormat(formats);

    // Query supported present modes
    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(devicePtr->physicalDevice, surface, &presentModeCount, nullptr);
    Vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(devicePtr->physicalDevice, surface, &presentModeCount, presentModes.data());

    // choosing defaults
    selectedPresentMode = ToVkPresentMode(presentMode, presentModes);

    // buffering mode
    imageCount = std::clamp(static_cast<u32>(bufferingMode), caps.minImageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = {width, height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = selectedPresentMode,
    };

    VK_CHECK(vkCreateSwapchainKHR(devicePtr->device, &createInfo, nullptr, &swapchain));

    CreateImages();
    LOG(Info, "Swapchain created successfully: {} x {}, format {}", width, height,
        static_cast<i32>(surfaceFormat.format));
    return {};
}

void VulkanSwapchain::Destroy()
{
    if (!vkDev || vkDev->device == VK_NULL_HANDLE) return;

    DestroySwapchainTextures();
    images.clear();

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vkDev->device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(vkDev->instance->instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
}

bool VulkanSwapchain::ResizeIfNeeded()
{
    u32 windowWidth = 0;
    u32 windowHeight = 0;
    Platform::GetWindowSize(handle, windowWidth, windowHeight);

    // if minimized (TODO: Win32 reports it earlier than SDL...)
    if (windowWidth == 0 || windowHeight == 0)
    {
        return false;
    }

    // if different
    if ((swapchain == VK_NULL_HANDLE) || (windowWidth != width) || (windowHeight != height) || needsRecreation)
    {
        Recreate();
        return false;
    }

    return true;
}
void VulkanSwapchain::CreateImages()
{
    imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(vkDev->device, swapchain, &imageCount, nullptr));
    Vector<VkImage> vkImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(vkDev->device, swapchain, &imageCount, vkImages.data()));

    images.resize(imageCount);

    TextureInfo info = {
        .extent = {width, height, 1},
        .type = ImageType::Image2D,
        .format = ToEngineFormat(surfaceFormat.format),
        .usage = ImageUsage::ColorAttachment,
        .sampleCount = SampleCount::X1
    };

    for (u32 i = 0; i < imageCount; ++i)
    {
        images[i].InitExternal(vkDev, vkImages[i], info);
        images[i].SetName(fmt::format("Swapchain Image: {}", i));
    }
}

void VulkanSwapchain::DestroySwapchainTextures()
{
    for (VulkanTexture& image : images)
    {
        image.Destroy();
    }
}

void VulkanSwapchain::SetVsyncMode(PresentMode mode)
{
    if (mode == presentMode) return;

    presentMode = mode;
    needsRecreation = true;
}

void VulkanSwapchain::SetBufferingMode(BufferingMode mode)
{
    if (mode == bufferingMode) return;

    bufferingMode = mode;
    needsRecreation = true;
}

bool VulkanSwapchain::Recreate()
{
    vkDeviceWaitIdle(vkDev->device);

    Platform::GetWindowSize(handle, width, height);

    DestroySwapchainTextures();

    // Query capabilities again
    VkSurfaceCapabilitiesKHR caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDev->physicalDevice, surface, &caps);

    // Platform::GetWindowSize(handle, width, height);
    width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);

    renderWidth = static_cast<u32>(width * renderScale);
    renderHeight = static_cast<u32>(height * renderScale);

    // Query surface formats again (in case monitor settings changed)
    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkDev->physicalDevice, surface, &formatCount, nullptr);
    Vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkDev->physicalDevice, surface, &formatCount, formats.data());

    surfaceFormat = PickSurfaceFormat(formats);

    // Same for present modes
    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkDev->physicalDevice, surface, &presentModeCount, nullptr);
    Vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkDev->physicalDevice, surface, &presentModeCount, presentModes.data());

    selectedPresentMode = ToVkPresentMode(presentMode, presentModes);

    // buffering mode
    imageCount = std::clamp(static_cast<u32>(bufferingMode), caps.minImageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = {width, height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = selectedPresentMode,
        .oldSwapchain = swapchain
    };

    VkSwapchainKHR newSwapchain;
    VK_CHECK(vkCreateSwapchainKHR(vkDev->device, &createInfo, nullptr, &newSwapchain));
    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vkDev->device, swapchain, nullptr);
    }

    swapchain = newSwapchain;

    CreateImages();
    needsRecreation = false;
	justRecreated = true;
    return true;
}

VkSurfaceFormatKHR VulkanSwapchain::PickSurfaceFormat(const Vector<VkSurfaceFormatKHR>& availableFormats) const
{
    if (preferHDR)
    {
        for (const auto& format : availableFormats)
        {
            if (format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
                format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
            {
                LOG(Info, "Selecting HDR10 Surface Format");
                return format;
            }
        }
    }

    for (const auto& format : availableFormats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    // 3. Absolute Fallback
    return availableFormats[0];
}

void VulkanSwapchain::NeedsReCreation()
{
    needsRecreation = true;
}

void VulkanSwapchain::SetRenderScale(f32 scale)
{
    // Clamp to safe boundaries (25% to 200% scaling factors)
    const f32 clampedScale = std::clamp(scale, 0.25f, 2.0f);
    if (renderScale == clampedScale) return;

    renderScale = clampedScale;

    vkDev->WaitIdle();

    // Recalculate dimensions relative to current physical window sizes
    renderWidth = static_cast<u32>(width * renderScale);
    renderHeight = static_cast<u32>(height * renderScale);

    needsRecreation = true;
    LOG(Info, "[Swapchain] Dynamic Render Scale updated: {:.2f}x ({} x {})", renderScale, renderWidth, renderHeight);
}

Result<u32> VulkanSwapchain::AcquireNextImage(GPUSemaphore* semaphore)
{
    // Ensure we are synchronized
    const auto vkSemaphore = static_cast<VulkanSemaphore*>(semaphore)->semaphore;
    u32 imageIndex = 0;

    const VkResult result = vkAcquireNextImageKHR(vkDev->device, swapchain, UINT64_MAX,
                                                  vkSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) return std::unexpected(VulkanSwapchainOutOfDate);
    if (result == VK_SUBOPTIMAL_KHR) return std::unexpected(Suboptimal);
    if (result != VK_SUCCESS) return std::unexpected(VulkanCommandBufferFailed);

    if (GPUTexture* currentSwapchainTex = GetImage(imageIndex))
    {
        currentSwapchainTex->currentLayout = TextureLayout::Unknown;
    }
    currentImageIndex = imageIndex;
    return imageIndex;
}

GPUTexture* VulkanSwapchain::GetCurrentImage()
{
    assert(currentImageIndex < images.size());
    return &images[currentImageIndex];
}

GPUTexture* VulkanSwapchain::GetImage(u32 index)
{
    assert(index < images.size());
    return &images[index];
}
