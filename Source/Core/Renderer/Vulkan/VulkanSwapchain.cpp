//
// Created by Orgest on 6/11/2025.
//

#include "VulkanSwapchain.h"

#include <algorithm>
#include <volk.h>
#include <windows.h>
#include <vulkan/vulkan_win32.h>

#include "Logger.h"
#include "Platform.h"
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

bool VulkanSwapchain::Init(VulkanDevice* device, WindowHandle handle)
{
	ZoneScopedN("Init Swapchain");
	this->device = device;
	this->handle = handle;

	LOG(Info, "Initializing Vulkan swapchain...");

#ifdef _WIN32
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = GetModuleHandle(nullptr),
		.hwnd = static_cast<HWND>(handle)
	};
	VK_CHECK(vkCreateWin32SurfaceKHR(device->instance->instance, &surfaceCreateInfo, nullptr, &surface));
	LOG(Info, "Created Win32 surface.");
#endif

	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice, surface, &caps);

	// the surface can sometimes bork
	if (caps.currentExtent.width == UNDEFINED_EXTENT)
	{
		LOG(Warning, "Surface extent undefined, using window size.");
		Platform::GetWindowSize(handle, width, height);
		width  = std::clamp(width,  caps.minImageExtent.width, caps.maxImageExtent.width);
		height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
	} else {
		width  = caps.currentExtent.width;
		height = caps.currentExtent.height;
	}

	// Query supported surface formats
	u32 formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, nullptr);
	Vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &formatCount, formats.data());
	LOG(Debug, "Found {} surface formats.", formatCount);


	// // Special case: driver says "choose anything"
	// if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	// {
	// 	surfaceFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	// }
	// else
	// {
	// 	// Prefer BGRA8_sRGB, then RGBA8_sRGB
	// 	surfaceFormat = formats[0]; // default
	// 	for (const auto& f : formats)
	// 	{
	// 		if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
	// 			f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
	// 		{
	// 			surfaceFormat = f;
	// 			break;
	// 		}
	// 	}
	// 	if (surfaceFormat.format != VK_FORMAT_B8G8R8A8_SRGB)
	// 	{
	// 		for (const auto& f : formats)
	// 		{
	// 			if (f.format == VK_FORMAT_R8G8B8A8_SRGB &&
	// 				f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
	// 			{
	// 				surfaceFormat = f;
	// 				break;
	// 			}
	// 		}
	// 	}
	// }


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
		.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface          = surface,
		.minImageCount    = imageCount,
		.imageFormat      = surfaceFormat.format,
		.imageColorSpace  = surfaceFormat.colorSpace,
		.imageExtent      = {width, height},
		.imageArrayLayers = 1,
		.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode      = selectedPresentMode,
		.clipped          = VK_TRUE,
		.oldSwapchain     = VK_NULL_HANDLE
	};

	VK_CHECK(vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapchain));

	CreateImages();
	CreateDepthImage();

	LOG(Info, "Swapchain created successfully: {} x {}, format {}", width, height, static_cast<int>(surfaceFormat.format));

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
		images[i] = VulkanImage(device, vkImages[i]);
		images[i].textureInfo.dimension = TextureDimension::TEXTURE_2D;
		images[i].textureInfo.extent.width = width;
		images[i].textureInfo.extent.height = height;
		images[i].textureInfo.mipLevels = 1;
		images[i].textureInfo.usage = ImageUsage::COLOR_ATTACHMENT;
		images[i].FillSubresoruceInfo();
		images[i].CreateImageView(surfaceFormat.format);
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
	VkSurfaceCapabilitiesKHR caps{};
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
		.imageExtent = { width, height },
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

	LOG(Info, "Swapchain recreated with present mode {} and format {}", (int)selectedPresentMode, (int)surfaceFormat.format);

	return true;
}


bool VulkanSwapchain::Resize()
{
	Platform::GetWindowSize(handle, width, height);
	if (width == 0 || height == 0) {
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
	for (VulkanImage& image : images) {
		if (image.imageView != VK_NULL_HANDLE) {
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
		.extent = { width, height, 1 },
		.mipLevels = 1,
		.format = TextureFormat::D32_SFLOAT,
		.dimension = TextureDimension::TEXTURE_2D,
		.usage = ImageUsage::DEPTH_STENCIL_ATTACHMENT | ImageUsage::SAMPLED
	};

	depthImage.Init(device, depthInfo);
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

