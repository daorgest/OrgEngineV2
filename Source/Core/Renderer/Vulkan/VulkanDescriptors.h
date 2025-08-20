//
// Created by Orgest on 6/24/2025.
//

#pragma once
#include <deque>
#include <optional>
#include <span>
#include <volk.h>

#include "RendererTypes.h"
#include "Vector.h"

namespace Renderer
{
	struct VulkanSampler;
	struct VulkanImage;
	struct VulkanBuffer;
	struct VulkanDevice;

	// Descriptor Layout Builder
	struct DescriptorLayoutBuilder
	{
		Vector<VkDescriptorSetLayoutBinding> bindings;

		DescriptorLayoutBuilder& AddBinding(u32 binding, DescriptorType type);
		VkDescriptorSetLayout Build(VkDevice device, ShaderStageFlags shaderStages, void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
		void Clear() { bindings.clear(); };
	};

	// Descriptor Writer
	struct VkDescriptorWriter
	{
		std::deque<VkDescriptorImageInfo> imageInfos;
		std::deque<VkDescriptorBufferInfo> bufferInfos;
		Vector<VkWriteDescriptorSet> writes;

		VkDescriptorWriter& WriteImage(u32 binding, std::optional<VulkanImage*> image, const VulkanSampler* sampler, DescriptorType type);
		VkDescriptorWriter& WriteImageArray(u32 binding, std::span<const VulkanImage*> images, DescriptorType type);
		VkDescriptorWriter& WriteBuffer(u32 binding, std::optional<VulkanBuffer*> buffer, DescriptorType type);

		void Clear();
		void UpdateSet(VkDevice device, VkDescriptorSet set);
	};

	// Growable Descriptor Allocator
	struct DescriptorAllocatorGrowable
	{
		struct PoolSizeRatio
		{
			VkDescriptorType type{};
			f32 ratio = 0;
		};

		void Init(VulkanDevice* device, u32 setsPerPool, std::span<PoolSizeRatio> poolRatios);
		void ResetPools();
		void DestroyPools();

		VkDescriptorSet Allocate(VkDescriptorSetLayout layout, void* pNext = nullptr);

	private:
		VkDescriptorPool GetPool(); // retrieves a ready pool or creates a new one
		VkDescriptorPool CreatePool(u32 setCount); // actually creates a Vulkan pool

		VulkanDevice* device = nullptr;
		Vector<PoolSizeRatio> ratios;
		Vector<VkDescriptorPool> fullPools;
		Vector<VkDescriptorPool> readyPools;
		u32 setsPerPool = 0;
	};
}
