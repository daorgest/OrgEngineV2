//
// Created by Orgest on 8/3/2025.
//

#pragma once
#include "../../../Engine/MeshData.h"
#include "Tools/Vector.h"

#include <glm/glm.hpp>

#include "../../../Engine/AABB.h"
#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"

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
		VulkanSampler* fallbackNormalSampler = nullptr;
	};

	struct VulkanMaterial
	{
		VulkanImage* colorImage = nullptr;
		VulkanSampler* sampler = nullptr;
		VulkanImage* normalImage = nullptr;
		DescriptorSet descriptorSet;

		// PBR Material Properties (from MTL file or defaults)
		glm::vec3 baseColor = glm::vec3(1.0f);
		f32 roughness = 0.5f;
		f32 metallic = 0.0f;
		f32 ior = 1.5f;
		f32 opacity = 1.0f;
		glm::vec3 emissive = glm::vec3(0.0f);
	};

	struct VulkanModel
	{
		Vector<VulkanMeshPart> parts;
		Vector<VulkanImage> images;
		Vector<VulkanSampler> samplers; // 1 sampler for now, but can extend if getting samplers from model source
		Vector<VulkanMaterial> materials;
		DescriptorLayout layout;
		DescriptorAllocatorGrowable descriptorPool;
		bool ownsLayout = true;

		VulkanBuffer vertexBuffer;
		VulkanBuffer indexBuffer;
		u64 vertexAddress = 0;

		// Materials soon...AABB, etc
		VulkanModel() = default;

		VulkanModel(VulkanDevice* device, LoadedModel& loadedModel, TextureOrFallback& fallback);
		void Destroy(const VulkanDevice* device);
	};


	struct ModelComponent
	{
		VulkanModel* model = nullptr;
		glm::mat4 transform = {1.0f};
		f32 roughness = 0.5f;  // Surface roughness [0=smooth, 1=rough]
		f32 metallic = 0.0f;   // Metallic property [0=dielectric, 1=metal]
	};

	struct DrawCache
	{
		VkPipelineLayout layout = VK_NULL_HANDLE;
		const VulkanMaterial* lastMat = nullptr;
		VkDescriptorSet lastMatSet = VK_NULL_HANDLE;
		VkBuffer lastIndex = VK_NULL_HANDLE;
	};

	struct VulkanMeshPart
	{
		AABB localBounds;
		u32 materialIndex = 0;
		u32 indexCount = 0;
		u32 firstIndex = 0;     // <-- NEW: Offset into the model's global index buffer
		u32 vertexOffset = 0;   // <-- NEW: Offset into the model's global vertex buffer
		glm::mat4 transform = glm::mat4(1.0f);

		void Draw(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, DescriptorSet materialSet) const;
	};
}
