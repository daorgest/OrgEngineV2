//
// Created by Orgest on 12/27/2025.
//

#pragma once
#include "RendererTypes.h"
#include "ShaderParams.h"
#include "../Core/Tools/Array.h"

namespace Constants
{
    // Scene Global Data
    inline const Vector<Binding> Scene = {
        {2, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(DebugUBO), 1, false},
        {3, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(CameraUBO), 1, false},
        {4, DescriptorType::StorageBuffer, ShaderStage::AllGraphics,    sizeof(LightUBO) * MAX_LIGHTS, 1, false},
        {5, DescriptorType::UniformBuffer, ShaderStage::AllGraphics,    sizeof(LightUBOCount), 1, false},
        {6, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(SceneUBO), 1, false}
    };

    // PBR Material
    inline const Vector<Binding> Material = {
        {0, DescriptorType::StorageBuffer, ShaderStage::AllGraphics, sizeof(MaterialProperties) * 1000, 1, false},
        {1, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics, 0, 1000, true}
    };

    // Instance Data SSBO (Set 3)
    inline const Vector<Binding> InstanceData = {
        {0, DescriptorType::StorageBuffer, ShaderStage::Vertex, sizeof(GPUInstanceSSBO) * 10000, 1, false}
    };

    // Skybox
    inline const Vector<Binding> Skybox = {
        {0, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics}
    };

    // Compute Shader
    inline const Vector<Binding> Compute = {
        {0, DescriptorType::StorageImage, ShaderStage::Compute, 0, 1, false}
    };
}
