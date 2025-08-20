//
// Created by Orgest on 8/3/2025.
//

#pragma once
#include "MeshData.h"
#include "Vector.h"
#include "volk.h"


#include <memory>

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
struct ArenaAllocator;

namespace Renderer
{
	struct DescriptorAllocatorGrowable;
	struct VulkanSampler;
	struct VulkanImage;
	struct VulkanMeshPart;
	struct VulkanDevice;
	struct VulkanBuffer;

	struct TextureFallback
	{
		VulkanImage* fallbackImage = nullptr;
		VulkanSampler* fallbackSampler = nullptr;
	};

	struct VulkanMaterial
	{
		VulkanImage* colorImage = nullptr;
		VulkanSampler* sampler = nullptr;
		VulkanImage* normalImage = nullptr;
		u32 materialIndex = 0;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	};

	struct VulkanModel
	{
		Vector<VulkanMeshPart> parts;
		Vector<VulkanImage> images;
		Vector<VulkanSampler> samplers;
		Vector<VulkanMaterial> materials;
		VkDescriptorSetLayout layout = VK_NULL_HANDLE;
		DescriptorAllocatorGrowable descriptorPool;

		// Materials soon...AABB, etc
		VulkanModel() = default;

		void LoadModel(VulkanDevice* device, const LoadedModel& loadedModel, TextureFallback* fallback);
		void Destroy(const VulkanDevice* device);
	};


	struct VulkanModelComponent
	{
		VulkanModel* model = nullptr;
		Mat4x4 transform;
	};

	struct VulkanMeshPart
	{
		VulkanBuffer vertexBuffer;
		VulkanBuffer indexBuffer;
		u64 vertexAddress;

		u32 indexCount = 0;
		u32 materialIndex = 0;
		std::string materialName = "albedo";

		void Destroy();
		bool Create(VulkanDevice* device, const MeshPart& mesh);
		void Draw(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, VkDescriptorSet materialSet) const;
	};
}
