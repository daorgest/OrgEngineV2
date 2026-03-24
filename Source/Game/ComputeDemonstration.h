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
    Renderer::DescriptorSet descriptorSet;
    std::unique_ptr<Renderer::GPUSampler> displaySampler;
    std::unique_ptr<Renderer::GPUPipeline> pipeline;
    std::unique_ptr<Renderer::GPUTexture> texture;
    std::unique_ptr<Renderer::GPUShader> shader;
    Renderer::DescriptorSet set;
    Extent2D lastExtent = { 0, 0 };

    void Init(Renderer::GPUDevice* device, Renderer::DescriptorAllocatorGrowable& descAlloc);
    void Resize(Renderer::GPUDevice* device, const Extent2D& newSize);
    void Execute(Renderer::GPUCommandBuffer* cmd, Extent2D extent, f32 elapsedTime, const glm::vec2& mousePos = {});
};
