//
// Created by Orgest on 1/30/2026.
//
#pragma once
#include "RenderInterface.h"
#include "VulkanDescriptors.h"
#include "glm/vec2.hpp"


struct ComputePushConstants {
    f32 time;
    glm::vec2 mousePos;
};

struct ComputeDemonstration
{
    Renderer::DescriptorLayout layout;
    std::unique_ptr<Renderer::GPUDescriptorSet> descriptorSet;

    std::unique_ptr<Renderer::GPUSampler> displaySampler;
    std::unique_ptr<Renderer::GPUPipeline> pipeline;
    std::unique_ptr<Renderer::GPUTexture> texture;
    std::shared_ptr<Renderer::GPUShader> shader;

    u64 texId = 0;
    Renderer::Extent2D lastExtent = { 0, 0 };

    void Init(Renderer::GPUDevice* device, Renderer::GPUShaderManager* shaderManager, Renderer::DescriptorAllocatorGrowable&
              descAlloc);
    void Resize(Renderer::GPUDevice* device, const Renderer::Extent2D& newSize);
    void Destroy();
    void Execute(Renderer::GPUCommandBuffer* cmd, Renderer::Extent2D extent, const ComputePushConstants& cPC) const;
    void DrawUI(Renderer::GPUDevice* device);
};
