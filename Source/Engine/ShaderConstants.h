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
    inline Array<Binding, 5> Scene = {
        {2, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(DebugUBO), 1, false},
        {3, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(CameraUBO), 1, false},
        {4, DescriptorType::StorageBuffer, ShaderStage::AllGraphics,    sizeof(LightUBO) * MAX_LIGHTS, 1, false},
        {5, DescriptorType::UniformBuffer, ShaderStage::AllGraphics,    sizeof(LightUBOCount), 1, false},
        {6, DescriptorType::UniformBuffer, ShaderStage::AllGraphics, sizeof(SceneUBO), 1, false}
    };

    // PBR Material
    inline Array<Binding, 2> Material = {
        {0, DescriptorType::StorageBuffer, ShaderStage::AllGraphics, sizeof(MaterialProperties) * 1000, 1, false},
        {1, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics, 0, 1000, true}
    };

    // Instance Data SSBO (Set 3)
    inline Array<Binding, 1> InstanceData = {
        {0, DescriptorType::StorageBuffer, ShaderStage::Vertex, sizeof(GPUInstanceSSBO) * 10000, 1, false}
    };

    // Skybox
    inline Array<Binding, 1> Skybox = {
        {0, DescriptorType::CombinedImageSampler, ShaderStage::AllGraphics}
    };

}
