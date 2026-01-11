//
// Created by Orgest on 6/15/2025.
//

#include "VulkanShader.h"

#include <span>
#include <volk.h>

#include "VulkanDevice.h"
#include "Tools/Logger.h"
#include "Tools/Vector.h"
#include "Tools/FileManager.h" // new: allocation-free file io

using namespace Renderer;


Result<Vector<u32>> LoadSPV(const char* path)
{
	auto fileSizeResult = FileManager::GetFileSize(path);
	if (!fileSizeResult)
	{
		LOG(Error, "Failed to get shader file size: {}", path);
		return std::unexpected(fileSizeResult.error());
	}

	const i32 fileSize = *fileSizeResult;
	if (fileSize <= 0 || fileSize % sizeof(u32) != 0)
	{
		LOG(Error, "Invalid SPIR-V file (bad size or alignment): {}", path);
		return std::unexpected(InvalidFileFormat);
	}

	Vector<u8> raw(static_cast<size_t>(fileSize));
	auto readResult = FileManager::ReadBinary(path, raw);
	if (!readResult)
	{
		LOG(Error, "Failed to read SPIR-V file: {}", path);
		return std::unexpected(readResult.error());
	}

	Vector<u32> buffer(fileSize / sizeof(u32));
	std::memcpy(buffer.data(), raw.data(), fileSize);
	return buffer;
}

Result<Vector<u32>> VulkanShader::ReadShaderFile(const char* filePath)
{
	auto result = LoadSPV(filePath);
	if (!result)
	{
		LOG(Error, "Failed to load shader file: {}", filePath);
		return std::unexpected(result.error());
	}
	return result;
}

// VulkanShader RHI Interface Implementation
Result<void> VulkanShader::Init(GPUDevice* gpuDevice, std::span<const u32> code, ShaderFormat shaderFormat)
{
	device = static_cast<VulkanDevice*>(gpuDevice);
	format = shaderFormat;

	const VkShaderModuleCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = code.size_bytes(),
		.pCode = code.data(),
	};

	const VkResult vkRes = vkCreateShaderModule(device->device, &createInfo, nullptr, &shader);
	if (vkRes != VK_SUCCESS)
	{
		shader = VK_NULL_HANDLE;
		return std::unexpected(ShaderCompileFailed);
	}

	return {};
}

void VulkanShader::Destroy()
{
	if (shader != VK_NULL_HANDLE && device)
	{
		vkDestroyShaderModule(device->device, shader, nullptr);
		shader = VK_NULL_HANDLE;
	}
}

VulkanShader::VulkanShader(VulkanDevice* dev, std::span<const u32> code, ShaderFormat fmt)
{
	Init(dev, code, fmt);
}
