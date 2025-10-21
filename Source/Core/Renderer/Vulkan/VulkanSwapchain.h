//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include "VulkanTexture.h"

#include "RendererTypes.h"
#include "Tools/Vector.h"

constexpr u32 UNDEFINED_EXTENT = UINT32_MAX;
constexpr u32 DOUBLE_BUFFERING = 2;
constexpr u32 TRIPLE_BUFFERING = 3;

namespace Renderer
{
	using WindowHandle = void*;

	struct VulkanDevice;
	struct VulkanSwapchain
	{
		VulkanDevice* device = nullptr;
		WindowHandle handle = nullptr; // Platform-native window (e.g. HWND on Windows)
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkPresentModeKHR selectedPresentMode;
		PresentMode presentMode = PresentMode::VSyncOn;
		BufferingMode bufferingMode = BufferingMode::Double;
		VkSurfaceFormatKHR surfaceFormat = { .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
		Vector<VulkanImage> images; // Color / Texture images

		VulkanImage depthImage;

		u32 imageCount = 0;
		u32 width = 0;
		u32 height = 0;
		bool needsRecreation = false;

		bool Init(VulkanDevice* device, WindowHandle handle);
		void CreateImages();
		void DestroyImageViews();
		void CreateDepthImage();
		void DestroyDepthImage() const;
		bool Resize();
		void VsyncEnable(PresentMode mode);
		void SetBufferingMode(BufferingMode mode);
		void Destroy();

		[[nodiscard]] const VulkanImage& GetImage(u32 index) const;
		bool Recreate();
		[[nodiscard]] Extent2D GetExtent() const { return { width, height }; }
		[[nodiscard]] VkFormat GetVkFormat() const { return surfaceFormat.format; }
	};
}
