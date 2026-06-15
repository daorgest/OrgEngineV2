//
// Created by Orgest on 6/24/2025.
//

#pragma once
#include <deque>
#include <volk.h>

#include "RendererTypes.h"
#include "RenderInterface.h"
#include "Tools/Vector.h"

// TODO Orgest: Trash this whole dang thing and use VK_EXT_descriptor_heap if wider support

namespace Renderer
{
    struct GPUTextureView;
    struct GPUBuffer;
    struct GPUDevice;
	struct GPUSampler;

	struct DescriptorLayout
	{
		VkDescriptorSetLayout vk = VK_NULL_HANDLE;
		operator VkDescriptorSetLayout() const noexcept { return vk; }

	    void Destroy(const GPUDevice* device) const;
	};

    struct DescriptorWriter
    {
        std::deque<VkDescriptorImageInfo> imageInfos;
        std::deque<VkDescriptorBufferInfo> bufferInfos;
        Vector<VkWriteDescriptorSet> writes;

        DescriptorWriter& WriteImage(u32 binding, const GPUTextureView* view, const GPUSampler* sampler,
                                     DescriptorType type, u32 arrayElement = 0, u32 count = 1);
        DescriptorWriter& WriteBuffer(u32 binding, const GPUBuffer* buffer, DescriptorType type, u32 arrayElement = 0,
                                      size_t size = 0, size_t offset = 0);

        void Clear();
        void UpdateSet(const GPUDevice* device, VkDescriptorSet set);
    };

    struct VulkanDescriptorSet final : GPUDescriptorSet
    {
        VkDescriptorSet vk = VK_NULL_HANDLE;
        DescriptorWriter writer;
        operator VkDescriptorSet() const noexcept { return vk; }

        VulkanDescriptorSet() = default;

        explicit VulkanDescriptorSet(const VkDescriptorSet handle) : vk(handle)
        {
        }

        void WriteBuffer(u32 binding, const GPUBuffer* buffer, DescriptorType type) override;
        void WriteTexture(u32 binding, GPUTextureView* texture, GPUSampler* sampler, DescriptorType type,
                          u32 arrayElement) override;
        void WriteTextureArray(u32 binding, Span<GPUTextureView*> textures, DescriptorType type) override;
        void Update(GPUDevice* device) override;
    };

	// Descriptor Layout Builder
    struct DescriptorLayoutBuilder
    {
        Vector<Binding> metadata;
        Vector<VkDescriptorSetLayoutBinding> vkBindings;

        DescriptorLayoutBuilder& AddBinding(const Binding& binding);
        DescriptorLayoutBuilder& AddBindings(Span<const Binding> bindings);
        DescriptorLayout Build(const GPUDevice* device, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
        DescriptorLayout BuildFromDesc(const GPUDevice* device, const DescriptorSetLayoutDesc& desc);
        void Clear() { metadata.clear(); vkBindings.clear(); };
    };

	struct PoolSizes
	{
		DescriptorType type = DescriptorType::Unknown;
        f32 ratio = 0;
    };

    // Growable Descriptor Allocator
    struct DescriptorAllocatorGrowable
	{
	    void Init(GPUDevice* inDevice, u32 inSetsPerPool, Span<const PoolSizes> poolRatios);
        void ResetPools();
        void DestroyPools();
	    VulkanDescriptorSet Allocate(DescriptorLayout layout, bool isBindless = false, u32 bindlessCount = 0);

	private:
		VkDescriptorPool GetPool(); // retrieves a ready pool or creates a new one
		VkDescriptorPool CreatePool(u32 setCount); // actually creates a Vulkan pool

	    GPUDevice* device = nullptr;
		Vector<PoolSizes> ratios;
		Vector<VkDescriptorPool> fullPools;
		Vector<VkDescriptorPool> readyPools;
		u32 setsPerPool = 0;
	};
}
