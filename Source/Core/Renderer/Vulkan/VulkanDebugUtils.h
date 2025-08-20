//
// Created by Orgest on 7/3/2025.
//

#pragma once
#include <volk.h>
#if VULKAN_DEBUG_MODE
namespace Renderer
{
	inline void NameObject(VkDevice device, VkObjectType type, uint64_t handle, const char* name)
	{
		VkDebugUtilsObjectNameInfoEXT info = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = type,
			.objectHandle = handle,
			.pObjectName = name,
		};

		vkSetDebugUtilsObjectNameEXT(device, &info);
	}

	inline void CmdBeginLabel(VkCommandBuffer cmd, const char* name, f32 r, f32 g, f32 b, f32 a = 1.0f)
	{
		VkDebugUtilsLabelEXT label = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = name,
			.color = {r, g, b, a},
		};

		vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
	}

	inline void CmdEndLabel(VkCommandBuffer cmd)
	{
		vkCmdEndDebugUtilsLabelEXT(cmd);
	}

	inline void CmdInsertLabel(VkCommandBuffer cmd, const char* name, f32 r, f32 g, f32 b, f32 a = 1.0f)
	{
		VkDebugUtilsLabelEXT label = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pLabelName = name,
			.color = {r, g, b, a},
		};

		vkCmdInsertDebugUtilsLabelEXT(cmd, &label);
	}
}

#else
namespace Renderer {
	inline void NameObject(VkDevice, VkObjectType, uint64_t, const char*) {}
	inline void CmdBeginLabel(VkCommandBuffer, const char*, f32, f32, f32, f32 = 1.0f) {}
	inline void CmdEndLabel(VkCommandBuffer) {}
	inline void CmdInsertLabel(VkCommandBuffer, const char*, f32, f32, f32, f32 = 1.0f) {}
}
#endif