//
// Created by Orgest on 6/15/2025.
//

#include "VulkanShader.h"

#include <volk.h>

#include "VulkanCheck.h"
#include "VulkanDevice.h"

using namespace Renderer;

VulkanShader::VulkanShader(VulkanDevice* inDev, Span<const u32> code, ShaderFormat fmt)
{
    device = inDev;
    format = fmt;

    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = code.size_bytes(),
        .pCode = code.data(),
    };

    VK_CHECK(vkCreateShaderModule(device->device, &createInfo, nullptr, &shader));
}

void VulkanShader::Destroy() const
{
	if (shader || device)
	{
		vkDestroyShaderModule(device->device, shader, nullptr);
	}
}