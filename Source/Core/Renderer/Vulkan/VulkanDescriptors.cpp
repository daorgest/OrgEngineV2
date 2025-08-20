//
// Created by Orgest on 6/24/2025.
//

#include "VulkanDescriptors.h"

#include <cmath>

#include "Logger.h"
#include "VulkanBuffer.h"
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"

using namespace Renderer;

// Layout Builder
DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBinding(u32 binding, DescriptorType type)
{
	const VkDescriptorSetLayoutBinding newBind
	{
		.binding = binding,
		.descriptorType = ToVk(type),
		.descriptorCount = 1
	};

	bindings.push_back(newBind);
	return *this;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice device, ShaderStageFlags stageFlags, void* pNext,
	VkDescriptorSetLayoutCreateFlags flags)
{
	for (auto& b : bindings)
	{
		b.stageFlags |= ToVk(stageFlags);
	}

	VkDescriptorSetLayoutCreateInfo info
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = pNext,
		.flags = flags,
		.bindingCount = static_cast<u32>(bindings.size()),
		.pBindings = bindings.data()
	};

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}


// Descriptor Writer
VkDescriptorWriter& VkDescriptorWriter::WriteImage(u32 binding, const std::optional<VulkanImage*> image, const VulkanSampler* sampler, DescriptorType type)
{
	VkDescriptorType vkType = ToVk(type);

	if (vkType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
		if (!image.has_value() || image.value() == nullptr)
			return *this;

		const VulkanImage* img = image.value();
		const VkImageLayout layout = IsDepthFormat(img->imageFormat)
		? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


		imageInfos.emplace_back(VkDescriptorImageInfo{
			.sampler     = VK_NULL_HANDLE,
			.imageView   = img->imageView,
			.imageLayout = layout,
		});
	}


	else if (vkType == VK_DESCRIPTOR_TYPE_SAMPLER) {
		if (sampler == nullptr)
			return *this;

		imageInfos.emplace_back(VkDescriptorImageInfo{
			.sampler     = sampler->sampler,
			.imageView   = VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		});
	}
	else {
		// Optional: warn if unsupported
		return *this;
	}

	writes.emplace_back(VkWriteDescriptorSet{
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding      = binding,
		.descriptorCount = 1,
		.descriptorType  = vkType,
		.pImageInfo      = &imageInfos.back()
	});

	return *this;
}

VkDescriptorWriter& VkDescriptorWriter::WriteImageArray(u32 binding, std::span<const VulkanImage*> images, DescriptorType type)
{
	if (images.empty()) return *this;

	const u32 start = static_cast<u32>(imageInfos.size());

	for (const VulkanImage* img : images)
	{
		imageInfos.emplace_back(VkDescriptorImageInfo{
			.sampler     = VK_NULL_HANDLE,
			.imageView   = img->imageView,
			.imageLayout = ToVk(img->imageLayout),
		});
	}

	writes.emplace_back(VkWriteDescriptorSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding = binding,
		.descriptorCount = static_cast<u32>(images.size()),
		.descriptorType = ToVk(type),
		.pImageInfo = &imageInfos[start]
	});

	return *this;
}

VkDescriptorWriter& VkDescriptorWriter::WriteBuffer(const u32 binding, const std::optional<VulkanBuffer*> buffer, DescriptorType type)
{
	if (!buffer.has_value() || buffer.value() == nullptr)
		return *this;

	const VulkanBuffer* buf = buffer.value();

	bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buf->buffer,
		.offset = 0,
		.range  = buf->info.size
	});

	writes.emplace_back(VkWriteDescriptorSet{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding      = binding,
		.descriptorCount = 1,
		.descriptorType  = ToVk(type),
		.pBufferInfo     = &bufferInfos.back()
	});

	return *this;
}

void VkDescriptorWriter::Clear()
{
	imageInfos.clear();
	bufferInfos.clear();
	writes.clear();
}

void VkDescriptorWriter::UpdateSet(VkDevice device, VkDescriptorSet set)
{
	for (auto& write : writes)
	{
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

// Descriptor Allocator Growable
void DescriptorAllocatorGrowable::Init(VulkanDevice* device, u32 setsPerPool, std::span<PoolSizeRatio> poolRatios)
{
	this->device = device;
	this->setsPerPool = setsPerPool;

	ratios.clear();
	ratios.reserve(poolRatios.size());

	for (const auto& ratio : poolRatios)
	{
		ratios.push_back(ratio);
	}
}

void DescriptorAllocatorGrowable::ResetPools()
{
	for (const auto& pool : readyPools)
	{
		vkResetDescriptorPool(device->device, pool, 0);
	}

	for (auto& pool : fullPools)
	{
		vkResetDescriptorPool(device->device, pool, 0);
		readyPools.push_back(pool);
	}

	fullPools.clear();
}

void DescriptorAllocatorGrowable::DestroyPools()
{
	for (auto& pool : readyPools)
	{
		vkDestroyDescriptorPool(device->device, pool, nullptr);
	}
	readyPools.clear();

	for (auto& pool : fullPools)
	{
		vkDestroyDescriptorPool(device->device, pool, nullptr);
	}
	fullPools.clear();
}
VkDescriptorSet DescriptorAllocatorGrowable::Allocate(VkDescriptorSetLayout layout, void* pNext)
{
	VkDescriptorSet set = VK_NULL_HANDLE;

	for (;;)
	{
		// Get or create a pool for allocation
		VkDescriptorPool pool = GetPool();
		if (pool == VK_NULL_HANDLE) {
			LOG(Error, "GetPool() returned null pool");
			return VK_NULL_HANDLE;
		}

		VkDescriptorSetAllocateInfo allocInfo = {
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext              = pNext,
			.descriptorPool     = pool,
			.descriptorSetCount = 1,
			.pSetLayouts        = &layout
		};

		VkResult result = vkAllocateDescriptorSets(device->device, &allocInfo, &set);

		// wtf
		if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
		{
			LOG(Warning, "Descriptor pool ran out of memory or fragmented for some reason. Creating a new pool.");
			// Mark the current pool as full and retry with a new pool
			fullPools.push_back(pool);
			continue; // loop will grab a new pool from GetPool()
		}

		if (result != VK_SUCCESS) {
			LOG(Error, "Failed to allocate descriptor set (VkResult: {})", static_cast<int>(result));
			return VK_NULL_HANDLE;
		}

		// Allocation succeeded — pool is still usable, so push it back for reuse
		readyPools.push_back(pool);
		return set;
	}
}


VkDescriptorPool DescriptorAllocatorGrowable::GetPool()
{
	if (!readyPools.empty())
	{
		VkDescriptorPool newPool = readyPools.back();
		readyPools.pop_back();
		return newPool;
	}

	VkDescriptorPool newPool = CreatePool(setsPerPool);
	if (newPool == VK_NULL_HANDLE) return VK_NULL_HANDLE;
	setsPerPool = std::min<u32>(setsPerPool, 4092); // its 4096 for the hard limit but this is for funny alignment reasons
	return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::CreatePool(u32 setCount)
{
	Vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(ratios.size());

	for (const auto& [type, ratio] : ratios)
	{
		const u32 count = static_cast<u32>(std::ceil(ratio * setCount));
		poolSizes.push_back({type, count});
	}

	VkDescriptorPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = setCount,
		.poolSizeCount = static_cast<u32>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool newPool;
	VkResult res = vkCreateDescriptorPool(device->device, &poolInfo, nullptr, &newPool);

	if (res == VK_SUCCESS)
	{
		LOG(Info, "Created new descriptor pool with capacity: {}", setCount);
		return newPool;
	}

	LOG(Error, "Failed to create descriptor pool.");
	return VK_NULL_HANDLE;
}