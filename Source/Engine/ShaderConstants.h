//
// Created by Orgest on 12/27/2025.
//

#pragma once
#include "RendererTypes.h"
#include "ShaderParams.h"

namespace Constants
{
    // Scene Global Data
    inline const Vector<Binding> Scene = {
        {2, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(DebugUBO) },
        {3, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(CameraUBO)},
        {4, DescriptorType::UniformBuffer, ShaderStage::AllGraphics,    sizeof(LightSceneData)},
        {6, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(SceneUBO)}

    };

    // PBR Material
    inline const Vector<Binding> MaterialBuffer = {
        {0, DescriptorType::StorageBuffer, ShaderStage::AllGraphics, sizeof(MaterialProperties) * 1000, 1, false}
    };

    // Instance Data SSBO (Set 3)
    inline const Vector<Binding> BindlessTextures = {
        {0, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics, 0, 1000, true}
    };

    // Set 3: Skybox
    inline const Vector<Binding> Skybox = {
        {0, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics}
    };

    // Set 4: Instance Data SSBO
    inline const Vector<Binding> InstanceData = {
        {0, DescriptorType::StorageBuffer, ShaderStage::AllGraphics, sizeof(GPUInstanceSSBO) * 10000, 1, false}
    };

    // Compute Shader
    inline const Vector<Binding> Compute = {
        {0, DescriptorType::StorageImage, ShaderStage::Compute, 0, 1, false}
    };
}
