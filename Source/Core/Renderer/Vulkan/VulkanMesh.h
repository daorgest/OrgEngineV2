//
// Created by Orgest on 8/3/2025.
//

#pragma once
#include "../../../Engine/MeshData.h"
#include "../../../Engine/AABB.h"
#include "Tools/Vector.h"

#include <glm/glm.hpp>

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanShaderBuffer.h"
#include "VulkanTexture.h"

struct SceneStats;

namespace Renderer
{
	struct VulkanPipeline;
	struct TextureDefaults;
	struct DescriptorAllocatorGrowable;
	struct VulkanSampler;
	struct VulkanTexture;
	struct VulkanDevice;
	struct VulkanBuffer;

	struct VulkanModel
	{
		Vector<MeshPart> parts;
		VulkanBuffer vertexBuffer;
		VulkanBuffer indexBuffer;
		u64 vertexBufferAddress = 0;

		Vector<MaterialProperties> materials;
		std::deque<VulkanTexture> images;

		std::unique_ptr<VulkanShaderBuffer> materialBuffer;
		AABB modelBounds;

		// Materials soon...AABB, etc
		VulkanModel() = default;
		VulkanModel(VulkanDevice* device, LoadedModel& loadedModel, DescriptorAllocatorGrowable& globalAllocator,
		            TextureDefaults& defaults);

		void Destroy();
	};

	enum class RenderPath
	{
		Standard,
		Instance,
		Indirect // No support yet
	};


	struct ModelComponent
	{
		VulkanModel* model = nullptr;
		glm::mat4 transform = {1.0f};

		RenderPath path = RenderPath::Standard;

		f32 roughness = 0.5f;  // Surface roughness [0=smooth, 1=rough]
		f32 metallic = 0.0f;   // Metallic property [0=dielectric, 1=metal]
		u32 materialIndex = 0;
	};

	struct DrawCache
	{
		VulkanPipeline* activePipeline = nullptr;
		GPUCommandBuffer* cmd = nullptr; // Current command buffer
		SceneStats* stats = nullptr; // Pointer to stats for tracking

		DescriptorSet lastMaterialSet;
		VulkanBuffer* lastIndexBuffer = nullptr;
		u32 frameIndex = 0;
	};
}
