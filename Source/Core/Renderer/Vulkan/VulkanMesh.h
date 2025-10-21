//
// Created by Orgest on 8/3/2025.
//

#pragma once
#include "MeshData.h"
#include "Tools/Vector.h"

#include <glm/glm.hpp>

#include "AABB.h"
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

	struct TextureOrFallback
	{
		VulkanImage* fallbackImage = nullptr;
		VulkanSampler* fallbackSampler = nullptr;
		VulkanImage* fallbackNormalImage = nullptr;
	};

	struct VulkanMaterial
	{
		VulkanImage* colorImage = nullptr;
		VulkanSampler* sampler = nullptr;
		VulkanImage* normalImage = nullptr;
		DescriptorSet descriptorSet;
	};

	struct VulkanModel
	{
		Vector<VulkanMeshPart> parts;
		Vector<VulkanImage> images;
		Vector<VulkanSampler> samplers;
		Vector<VulkanMaterial> materials;
		DescriptorLayout layout;
		DescriptorAllocatorGrowable descriptorPool;

		AABB modelAABB;

		// Materials soon...AABB, etc
		VulkanModel() = default;

		VulkanModel(VulkanDevice* device, LoadedModel& loadedModel, TextureOrFallback& fallback);
		void Destroy(const VulkanDevice* device);
	};


	struct ModelComponent
	{
		VulkanModel* model = nullptr;
		glm::mat4 transform = {1.0f};
	};

	struct DrawCache {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		const VulkanMaterial* lastMat = nullptr;   // optional (keep if you want)
		VkDescriptorSet      lastMatSet = VK_NULL_HANDLE;  // <-- add this
		VkBuffer             lastIndex = VK_NULL_HANDLE;
		VkDeviceSize         lastIndexOffset = ~VkDeviceSize{0};
	};

	struct VulkanMeshPart
	{
		VulkanBuffer vertexBuffer;
		VulkanBuffer indexBuffer;
		u64 vertexAddress = 0;
		u32 indexCount = 0;
		u32 materialIndex = 0;

		glm::mat4 transform = {1.0f};
		AABB localBounds;

		void Destroy();
		bool Create(VulkanDevice* device, const MeshPart& mesh);
		void Draw(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, DescriptorSet materialSet) const;
	};
}
