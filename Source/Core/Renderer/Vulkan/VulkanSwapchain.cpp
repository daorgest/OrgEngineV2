//
// Created by Orgest on 6/11/2025.
//

#include "VulkanSwapchain.h"

#include <algorithm>
#define VOLK_IMPLEMENTATION
#include <volk.h>
#include <windows.h>

#include "Platform.h"
#include "VulkanCheck.h"
#include "VulkanCommandBuffer.h"
#include "VulkanConvert.h"
#include "VulkanDebugUtils.h"
#include "VulkanDevice.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"
#if ENGINE_PLATFORM_SDL
#include "SDL3/SDL_vulkan.h"
#else
#include <vulkan/vulkan_win32.h>
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
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
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
        LOG(Warning, "Surface extent undefined, using window size.");
        Platform::GetWindowSize(handle, width, height);
        width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    else
    {
        width = caps.currentExtent.width;
        height = caps.currentExtent.height;
    }

    // Query supported surface formats
    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(devicePtr->physicalDevice, surface, &formatCount, nullptr);
    Vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(devicePtr->physicalDevice, surface, &formatCount, formats.data());
    LOG(Debug, "Found {} surface formats.", formatCount);

    // Query supported present modes
    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(devicePtr->physicalDevice, surface, &presentModeCount, nullptr);
    Vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(devicePtr->physicalDevice, surface, &presentModeCount, presentModes.data());
    LOG(Debug, "Found {} present modes.", presentModeCount);

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
    CreateDepthImage();

    LOG(Info, "Swapchain created successfully: {} x {}, format {}", width, height,
        static_cast<i32>(surfaceFormat.format));
    return {};
}

void VulkanSwapchain::Destroy()
{
    images.clear();
    depthTexture.Destroy();

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

    // if minimized
    if (windowWidth == 0 || windowHeight == 0)
    {
        return false;
    }

    // if different
    if ((windowWidth != width) || (windowHeight != height))
    {
        Recreate();
        return false;
    }

    return true;
}

GPUTexture* VulkanSwapchain::GetCurrentImage()
{
    if (currentImageIndex < images.size())
    {
        return &images[currentImageIndex];
    }
    return nullptr;
}


TextureFormat VulkanSwapchain::GetFormat() const
{
    // Convert VkFormat to TextureFormat
    switch (surfaceFormat.format)
    {
    case VK_FORMAT_B8G8R8A8_SRGB: return TextureFormat::BGRA8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM: return TextureFormat::BGRA8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB: return TextureFormat::RGBA8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM: return TextureFormat::RGBA8_UNORM;
    default: return TextureFormat::RGBA8_SRGB;
    }
}

void VulkanSwapchain::CreateImages()
{
    imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(vkDev->device, swapchain, &imageCount, nullptr));
    Vector<VkImage> vkImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(vkDev->device, swapchain, &imageCount, vkImages.data()));

    images.resize(imageCount);

    for (u32 i = 0; i < imageCount; ++i)
    {
        images[i] = {vkDev, vkImages[i]};
        images[i].textureInfo.dimension = TextureDimension::Texture2D;
        images[i].textureInfo.extent.width = width;
        images[i].textureInfo.extent.height = height;
        images[i].textureInfo.mipLevels = 1;
        images[i].textureInfo.usage = ImageUsage::ColorAttachment;
        images[i].FillSubresourceInfo();
        images[i].CreateImageView(surfaceFormat.format);

        // Debug
        char nameBuffer[32];
        snprintf(nameBuffer, sizeof(nameBuffer), "Swapchain Image %u", i);
        images[i].SetName(nameBuffer);
    }
}

void VulkanSwapchain::DestroyImageViews()
{
    for (VulkanTexture& image : images)
    {
        if (image.imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(vkDev->device, image.imageView, nullptr);
            image.imageView = VK_NULL_HANDLE;
        }
    }
}


void VulkanSwapchain::CreateDepthImage()
{
    if (vkDev == nullptr)
    {
        LOG(Error, "VulkanSwapchain: Cannot create depth image, device is nullptr.");
        return;
    }

    TextureInfo depthInfo = {
        .extent = {width, height, 1},
        .format = TextureFormat::D32_SFLOAT,
        .usage = ImageUsage::DepthStencil | ImageUsage::Sampled
    };

    depthTexture.Init(vkDev, depthInfo);
    depthTexture.SetName("Swapchain Depth Image");
    depthTexture.imageLayout = vkDev->useUnifiedLayout ?
                               TextureLayout::General :
                               TextureLayout::DepthWrite;
}

void VulkanSwapchain::DestroyDepthImage()
{
    depthTexture.Destroy();
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

    DestroyImageViews();
    DestroyDepthImage();

    // Query capabilities again
    VkSurfaceCapabilitiesKHR caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDev->physicalDevice, surface, &caps);

    if (caps.currentExtent.width != 0xFFFFFFFF)
    {
        width = caps.currentExtent.width;
        height = caps.currentExtent.height;
    }
    else
    {
        u32 w, h;
        Platform::GetWindowSize(handle, w, h);
        width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
        height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // Query surface formats again (in case monitor settings changed)
    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkDev->physicalDevice, surface, &formatCount, nullptr);
    Vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkDev->physicalDevice, surface, &formatCount, formats.data());

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
        .clipped = VK_TRUE,
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
    CreateDepthImage();
    needsRecreation = false;
    return true;
}

GPUTexture* VulkanSwapchain::GetImage(u32 index)
{
    return &images[index];
}

Result<u32> VulkanSwapchain::AcquireNextImage(GPUSemaphore* semaphore)
{
    if (!semaphore) return std::unexpected(VulkanInvalidState);
    const auto vkSemaphore = static_cast<VulkanSemaphore*>(semaphore)->semaphore;
    u32 imageIndex = 0;

    const VkResult result = vkAcquireNextImageKHR(vkDev->device, swapchain, UINT64_MAX,
                                                  vkSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return std::unexpected(VulkanSwapchainOutOfDate);
    }

    if (result == VK_SUBOPTIMAL_KHR)
    {
        currentImageIndex = imageIndex;
        return std::unexpected(Suboptimal);
    }

    if (result != VK_SUCCESS)
    {
        return std::unexpected(VulkanCommandBufferFailed);
    }

    currentImageIndex = imageIndex;
    return imageIndex;
}