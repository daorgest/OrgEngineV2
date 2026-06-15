//
// Created by Orgest on 6/10/2025.
//

#pragma once
#include <volk.h>
#include "RenderInterface.h"
namespace Renderer
{
	struct VulkanInstance final : GPUInterface
	{
		bool Init() override;
		void Destroy() override;

		VkInstance instance = VK_NULL_HANDLE;
		VkApplicationInfo appInfo = {};
#ifdef VULKAN_DEBUG_MODE
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
#endif
	};
} // namespace Renderer
