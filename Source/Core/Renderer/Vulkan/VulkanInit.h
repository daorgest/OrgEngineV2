//
// Created by Orgest on 6/10/2025.
//

#pragma once
#include "Tools/Logger.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include "RendererTypes.h"
#include "RenderInterface.h"
#include "volk.h"
#include "VulkanCheck.h"
#include "VulkanDebugUtils.h"
namespace Renderer
{
	struct VulkanInstance final : GPUInterface
	{
		VkInstance instance = VK_NULL_HANDLE;
		VkApplicationInfo appInfo;
#ifdef VULKAN_DEBUG_MODE
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
#endif
		bool Init();
		void Destroy() const;

	};

	struct VulkanDevice final : GPUDevice
	{
		VkDevice device = VK_NULL_HANDLE;
		VulkanInstance* instance = nullptr;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties deviceProperties = {};
		GPUDeviceDesc deviceDesc;
		VmaAllocator allocator = nullptr;
		VkQueue graphicsQueue = VK_NULL_HANDLE;;
		u32 graphicsQueueIndex = 0;
		bool Init(VulkanInstance* inst);
		void Destroy();

		// Immediate GPU Commands
		VkFence         immFence = VK_NULL_HANDLE;
		VkCommandBuffer immCommandBuffer = VK_NULL_HANDLE;
		VkCommandPool   immCommandPool = VK_NULL_HANDLE;

		bool InitImmediate();
		static std::string DecodeDriverVersion(u32 driverVersion, GPUVendor vendor);

		template<typename Func>
		void ImmediateSubmit(Func&& function, const char* labelName = "Immediate Submit", f32 r = 0.5f, f32 g = 0.5f, f32 b = 0.5f)
		{
			vkResetFences(device, 1, &immFence);
			vkResetCommandBuffer(immCommandBuffer, 0);

			VkCommandBuffer cmd = immCommandBuffer;

			VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};

			VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
#if VULKAN_DEBUG_MODE
			CmdBeginLabel(cmd, labelName, r, g, b);
#endif

			function(cmd);

#if VULKAN_DEBUG_MODE
			CmdEndLabel(cmd);
#endif

			VK_CHECK(vkEndCommandBuffer(cmd));

			VkCommandBufferSubmitInfo cmdInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.commandBuffer = cmd,
				.deviceMask = 0
			};
			VkSubmitInfo2 submitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
				.commandBufferInfoCount = 1,
				.pCommandBufferInfos = &cmdInfo
			};
			vkQueueSubmit2(graphicsQueue, 1, &submitInfo, immFence);
			vkWaitForFences(device, 1, &immFence, VK_TRUE, UINT64_MAX);
		}
	};
}
