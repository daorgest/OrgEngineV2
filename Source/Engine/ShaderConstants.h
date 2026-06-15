//
// Created by Orgest on 12/27/2025.
//

#pragma once
#include "RendererTypes.h"
#include "ShaderParams.h"

namespace Renderer::Constants
{
    // 1. Scene Global Data
    static constexpr Array<Binding, 5> Scene = {
        {
            {2, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(Engine::DebugUBO)},
            {3, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(Engine::CameraUBO)},
            {4, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(Engine::LightSceneData)},
            {5, DescriptorType::UniformBuffer, ShaderStage::Fragment, sizeof(Engine::VisibilityVolumeConstants)},
            {6, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(Engine::SceneUBO)}
        }
    };

    // 2. Bounding Box Constants
    static constexpr Array<Binding, 1> BoundingBox = {
        {
            {0, DescriptorType::StorageBuffer, ShaderStage::AllGraphics, sizeof(Engine::BBoxPush)}
        }
    };

    // 3. PBR Material
    static constexpr Array<Binding, 1> MaterialBuffer = {
        {
            {
                0, DescriptorType::StorageBuffer, ShaderStage::AllGraphics,
                sizeof(Engine::MaterialProperties) * MAX_MATERIAL_INSTANCES
            }
        }
    };

    // 4. 2D Bindless Textures
    static constexpr Array<Binding, 1> BindlessTextures2D = {
        {
            {0, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics, 0, MAX_BINDLESS_TEXTURES, true}
        }
    };

    // 5. Skybox
    static constexpr Array<Binding, 1> Skybox = {
        {
            {0, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics}
        }
    };

    // 6. Instance Data SSBO
    static constexpr Array<Binding, 1> InstanceData = {
        {
            {
                0, DescriptorType::StorageBuffer, ShaderStage::AllGraphics,
                sizeof(Engine::GPUInstanceSSBO) * MAX_MESH_INSTANCES
            }
        }
    };

    // 7. 3D Bindless Textures
    static constexpr Array<Binding, 1> BindlessTextures3D = {
        {
            {0, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics, 0, MAX_BINDLESS_TEXTURES, true}
        }
    };

    // 8. Compute Shader
    static constexpr Array<Binding, 1> Compute = {
        {
            {0, DescriptorType::StorageImage, ShaderStage::Compute}
        }
    };
}
