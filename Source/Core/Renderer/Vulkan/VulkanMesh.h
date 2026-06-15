//
// Created by Orgest on 8/3/2025.
//

#pragma once
#include "../../../Engine/MeshData.h"

#include "RenderInterface.h"
#include "VulkanDescriptors.h"
#include "Tools/AssetPool.h"

struct SceneStats;

namespace Renderer
{
    struct BindlessManager;
	struct DescriptorAllocatorGrowable;

    // Just a method now :3
    Result<GPUModel> CreateVulkanModel(GPUDevice* device, LoadedModel& loadedModel, BindlessManager& bindless,
                                       DescriptorAllocatorGrowable& allocator);
    // ECS? yeah...
    struct TransformComponent
    {
        glm::mat4 worldMatrix = glm::mat4(1.0f);
    };

    struct RenderPathComponent
    {
        RenderPath path = RenderPath::Standard;
    };

    struct MaterialComponent
    {
        u32 materialIndex = 0;
        f32 roughness = 1.0f;
        f32 metallic = 1.0f;
        glm::vec3 tint = glm::vec3(1.0f);
    };

    struct ModelComponent
    {
        ResourceHandle<GPUModel> modelHandle;
        TransformComponent transform;
        MaterialComponent material;
        RenderPathComponent renderPath;
    };

    struct DrawCache
    {
        // Pointers to currently bound RHI objects
        GPUPipeline* activePipeline = nullptr;
        GPUBuffer* lastIndexBuffer = nullptr;

        VkDescriptorSet lastMaterialSet = VK_NULL_HANDLE;

        // Context data for the draw calls
        GPUCommandBuffer* cmd = nullptr;
        SceneStats* stats = nullptr;

        void Flush() {
            activePipeline = nullptr;
            lastIndexBuffer = nullptr;
        }
    };
}
