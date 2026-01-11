//
// Created by Orgest on 6/10/2025.
//

#pragma once

#include <vk_mem_alloc.h>
#include "RenderInterface.h"
namespace Renderer
{
	struct VulkanInstance final : GPUInterface
	{
		bool Init() override;
		void Destroy() override;

		// Vulkan-specific accessors
		[[nodiscard]] VkInstance GetVkInstance() const noexcept { return instance; }

		// Public Vulkan handles
		VkInstance instance = VK_NULL_HANDLE;
		VkApplicationInfo appInfo = {};

#ifdef VULKAN_DEBUG_MODE
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
#endif
	};
} // namespace Renderer
