//
// Created by Orgest on 6/11/2025.
//

#include "VulkanSwapchain.h"

#include <algorithm>
#include <volk.h>
#include <windows.h>
#include <vulkan/vulkan_win32.h>

#include "Platform.h"
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "VulkanDebugUtils.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"
#if ENGINE_PLATFORM_SDL
#include "SDL3/SDL_vulkan.h"
#endif
#include "Tools/Logger.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

bool VulkanSwapchain::Init(VulkanDevice* devicePtr, WindowHandle windowHandle_)
{
    ZoneScopedN("Init Swapchain");
    this->device = devicePtr;
    this->handle = windowHandle_;

    LOG(Info, "Initializing Vulkan swapchain...");

#if ENGINE_PLATFORM_WIN32
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = static_cast<HWND>(handle)
    };
    VK_CHECK(vkCreateWin32SurfaceKHR(device->instance->instance, &surfaceCreateInfo, nullptr, &surface));
    LOG(Info, "Created Win32 Vulkan surface.");
#elif ENGINE_PLATFORM_SDL
    if (!SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(handle), device->instance->instance, nullptr, &surface))
    {
        LOG(Error, "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return false;
    }
    LOG(Info, "Created SDL3 Vulkan surface.");

#else
#error "Unsupported platform for Vulkan surface creation"
#endif

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice, surface, &caps);

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
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, nullptr);
    Vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, formats.data());
    LOG(Debug, "Found {} surface formats.", formatCount);

    // Query supported present modes
    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, nullptr);
    Vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, presentModes.data());
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

    VK_CHECK(vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapchain));

    CreateImages();
    CreateDepthImage();

    LOG(Info, "Swapchain created successfully: {} x {}, format {}", width, height,
        static_cast<int>(surfaceFormat.format));

    return true;
}

void VulkanSwapchain::CreateImages()
{
    imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device->device, swapchain, &imageCount, nullptr));
    Vector<VkImage> vkImages(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device->device, swapchain, &imageCount, vkImages.data()));

    images.resize(imageCount);

    for (u32 i = 0; i < imageCount; ++i)
    {
        images[i] = {device, vkImages[i]};
        images[i].textureInfo.dimension = TextureDimension::Texture2D;
        images[i].textureInfo.extent.width = width;
        images[i].textureInfo.extent.height = height;
        images[i].textureInfo.mipLevels = 1;
        images[i].textureInfo.usage = ImageUsage::ColorAttachment;
        images[i].FillSubresourceInfo();
        images[i].CreateImageView(surfaceFormat.format);
#if VULKAN_DEBUG_MODE

        char nameBuffer[64];
        snprintf(nameBuffer, sizeof(nameBuffer), "Color Image[%u]", i);
        Renderer::NameObject(device->device, VK_OBJECT_TYPE_IMAGE, (uint64_t)images[i].image, nameBuffer);
        snprintf(nameBuffer, sizeof(nameBuffer), "Color Image View[%u]", i);
        Renderer::NameObject(device->device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)images[i].imageView, nameBuffer);
#endif
    }
}

const VulkanImage& VulkanSwapchain::GetImage(u32 index) const
{
    return images[index];
}

bool VulkanSwapchain::Recreate()
{
    vkDeviceWaitIdle(device->device);

    // Query capabilities again
    VkSurfaceCapabilitiesKHR caps = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice, surface, &caps);

    // Query surface formats again (in case monitor settings changed)
    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, nullptr);
    Vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, formats.data());


    // surfaceFormat = formats[0];
    // for (const auto& f : formats) {
    // 	if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
    // 		surfaceFormat = f;
    // 		break;
    // 	}
    // }
    // texFormat = static_cast<TextureFormat>(surfaceFormat.format);

    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, nullptr);
    Vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, presentModes.data());
    selectedPresentMode = ToVkPresentMode(presentMode, presentModes);

    // buffering mode
    imageCount = std::clamp(static_cast<u32>(bufferingMode), caps.minImageCount, caps.maxImageCount);

    DestroyImageViews();
    DestroyDepthImage();

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
    VK_CHECK(vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &newSwapchain));
    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device->device, swapchain, nullptr);
    }

    swapchain = newSwapchain;

    CreateImages();
    CreateDepthImage();
    needsRecreation = false;
    return true;
}


bool VulkanSwapchain::Resize()
{
    Platform::GetWindowSize(handle, width, height);
    if (width == 0 || height == 0)
    {
        return false;
    }

    Recreate();

    LOG(Info, "Swapchain resized: {} x {}, format {}", width, height, static_cast<int>(surfaceFormat.format));
    return true;
}

void VulkanSwapchain::VsyncEnable(PresentMode mode)
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


void VulkanSwapchain::DestroyImageViews()
{
    for (VulkanImage& image : images)
    {
        if (image.imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device->device, image.imageView, nullptr);
            image.imageView = VK_NULL_HANDLE;
        }
    }
}

void VulkanSwapchain::CreateDepthImage()
{
    if (device == nullptr)
    {
        LOG(Error, "VulkanSwapchain: Cannot create depth image, device is nullptr.");
        return;
    }

    TextureInfo depthInfo = {
        .extent = {width, height, 1},
        .mipLevels = 1,
        .format = TextureFormat::D32_SFLOAT,
        .dimension = TextureDimension::Texture2D,
        .usage = ImageUsage::DepthStencil | ImageUsage::Sampled
    };

    depthImage.Init(device, depthInfo);

#if VULKAN_DEBUG_MODE
    // Name the depth image for debugging
    Renderer::NameObject(device->device, VK_OBJECT_TYPE_IMAGE, (uint64_t)depthImage.image, "Depth");
    Renderer::NameObject(device->device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)depthImage.imageView, "Depth View");
#endif
}

void VulkanSwapchain::DestroyDepthImage() const
{
    if (depthImage.imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->device, depthImage.imageView, nullptr);
    }

    if (depthImage.image != VK_NULL_HANDLE)
    {
        vmaDestroyImage(device->allocator, depthImage.image, depthImage.allocation);
    }
}

void VulkanSwapchain::Destroy()
{
    DestroyImageViews();
    DestroyDepthImage();

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device->device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(device->instance->instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
}

// VulkanSwapchain RHI Interface Implementation
bool VulkanSwapchain::Init(GPUDevice* gpuDevice, void* windowHandle)
{
    // Cast to VulkanDevice (safe because we control the backend)
    auto* vkDevice = static_cast<VulkanDevice*>(gpuDevice);
    return Init(vkDevice, windowHandle);
}

u32 VulkanSwapchain::AcquireNextImage(void* semaphore)
{
    auto* vkSemaphore = static_cast<VkSemaphore>(semaphore);
    u32 imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device->device, swapchain, UINT64_MAX,
                                            vkSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        needsRecreation = true;
        return UINT32_MAX; // Signal recreation needed
    }

    currentImageIndex = imageIndex;
    return imageIndex;
}

void VulkanSwapchain::Present(u32 imageIndex, void* waitSemaphore)
{
    auto* vkWaitSemaphore = static_cast<VkSemaphore>(waitSemaphore);

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vkWaitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(device->graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        needsRecreation = true;
    }
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