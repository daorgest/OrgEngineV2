//
// Created by Orgest on 6/24/2025.
//

#pragma once
#include <deque>
#include <span>
#include <volk.h>

#include "RendererTypes.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct GPUTexture;
	struct GPUSampler;
	struct VulkanBuffer;
	struct VulkanDevice;

	struct DescriptorLayout
	{
		VkDescriptorSetLayout vk = VK_NULL_HANDLE;
		operator VkDescriptorSetLayout() const noexcept { return vk; }

		void Destroy(const VulkanDevice* device) const;
	};

	struct DescriptorSet
	{
		VkDescriptorSet vk = VK_NULL_HANDLE;
		operator VkDescriptorSet() const noexcept { return vk; }

		void Destroy(const VulkanDevice* device);
	};

	// Descriptor Layout Builder
	struct DescriptorLayoutBuilder
	{
		Vector<Binding> metadata;
		Vector<VkDescriptorSetLayoutBinding> vkBindings;

		DescriptorLayoutBuilder& AddBinding(const Binding& binding);
		DescriptorLayoutBuilder& AddBindings(std::span<const Binding> bindings);
		DescriptorLayout Build(const VulkanDevice* device, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
		void Clear() { metadata.clear(); };
	};

	// Descriptor Writer
	struct DescriptorWriter
	{
		std::deque<VkDescriptorImageInfo> imageInfos;
		std::deque<VkDescriptorBufferInfo> bufferInfos;
		Vector<VkWriteDescriptorSet> writes;

		DescriptorWriter& WriteCombinedImage(u32 binding, const GPUTexture* image, const GPUSampler* sampler, u32 arrayElement = 1);
		DescriptorWriter& WriteImage(u32 binding, const GPUTexture* image, const GPUSampler* sampler,
		                             DescriptorType type);
		DescriptorWriter& WriteImageArray(u32 binding, std::span<const GPUTexture*> images, DescriptorType type);
		DescriptorWriter& WriteBuffer(u32 binding, const VulkanBuffer* buffer, DescriptorType type);
		DescriptorWriter& WriteBuffer(u32 binding, const VulkanBuffer* buffer, size_t size, size_t offset, DescriptorType type);
		DescriptorWriter& WriteBuffers(std::span<const Binding> bindings, const VulkanBuffer* megaBuffer,
		                               std::span<const u32> offsets);

		void Clear();
		void UpdateSet(const VulkanDevice* device, DescriptorSet set);
	};

	struct PoolSizes
	{
		DescriptorType type = DescriptorType::Unknown;
		f32 ratio = 0;
	};

	// Growable Descriptor Allocator
	struct DescriptorAllocatorGrowable
	{
		void Init(VulkanDevice* device, u32 setsPerPool, std::span<PoolSizes> poolRatios);
		void ResetPools();
		void DestroyPools();

		DescriptorSet Allocate(DescriptorLayout layout, bool isBindless = false, u32 bindlessCount = 0);

	private:
		VkDescriptorPool GetPool(); // retrieves a ready pool or creates a new one
		VkDescriptorPool CreatePool(u32 setCount); // actually creates a Vulkan pool

		VulkanDevice* device = nullptr;
		Vector<PoolSizes> ratios;
		Vector<VkDescriptorPool> fullPools;
		Vector<VkDescriptorPool> readyPools;
		u32 setsPerPool = 0;
	};
}
