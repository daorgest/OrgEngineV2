//
// Created by Orgest on 1/30/2026.
//

#include "ComputeDemonstration.h"

#include "imgui_impl_vulkan.h"
#include "ShaderConstants.h"
#include "VulkanPipeline.h"
#include "VulkanTexture.h"

void ComputeDemonstration::Init(Renderer::GPUDevice* device, Renderer::DescriptorAllocatorGrowable& descAlloc)
{
    shader = device->CreateShaderPath("Shaders/gradiant_comp.spv");

    Renderer::DescriptorLayoutBuilder b;
    layout = b.AddBindings(Constants::Compute).Build(device);
    descriptorSet = descAlloc.Allocate(layout);

    // Initial default size
    Resize(device, {1280, 720});
}

void ComputeDemonstration::Resize(Renderer::GPUDevice* device, const Extent2D& newSize)
{
    if (newSize.width == lastExtent.width && newSize.height == lastExtent.height) return;
    if (newSize.width == 0 || newSize.height == 0) return;

    TextureInfo textureInfo = {
        .extent = {newSize.width, newSize.height},
        .format = TextureFormat::RGBA8_UNORM,
        .usage = ImageUsage::Sampled | ImageUsage::Storage
    };

    texture = device->CreateTexture(textureInfo);
    texture->SetName("Compute Output");

    device->ImmediateSubmit([&](Renderer::GPUCommandBuffer* cmd)
    {
        cmd->TransitionLayout(texture.get(), TextureLayout::Unknown, TextureLayout::General);
    });

    Renderer::DescriptorWriter()
        .WriteImage(0, texture.get(), nullptr, DescriptorType::StorageImage)
        .UpdateSet(device, descriptorSet);

    SamplerInfo samplerInfo;
    displaySampler = device->CreateSampler(samplerInfo);
    auto* vkTex = static_cast<Renderer::VulkanTexture*>(texture.get());
    auto* vkSampler = static_cast<Renderer::VulkanSampler*>(displaySampler.get());
    if (set.vk != 0)
    {
        // This will push the old index onto our new free-list
        ImGui_ImplVulkan_RemoveTexture(set.vk);
    }

    // 2. RECONSTRUCT CREATION INFOS AND ALLOCATE
    // (Pulls from the free-list we just populated!)
    set.vk = ImGui_ImplVulkan_AddTextureHeap(
        &vkSampler->createInfo,
        &vkTex->imageViewCreateInfo,
        VK_IMAGE_LAYOUT_GENERAL
    );


    lastExtent = newSize;
}


void ComputeDemonstration::Execute(Renderer::GPUCommandBuffer* cmd, const Extent2D extent, const f32 elapsedTime,
                                   const glm::vec2& mousePos)
{
    cmd->TransitionLayout(texture.get(), TextureLayout::General);
    cmd->BindPipeline(pipeline.get());
    cmd->BindDescriptorSet(&descriptorSet, 0, pipeline.get());
    const ComputePushConstants cPC =
    {
        .time = elapsedTime,
        .mousePos = mousePos,
    };
    cmd->PushConstants(pipeline.get(), ShaderStage::Compute, 0, sizeof(ComputePushConstants), &cPC);
    const u32 groupCountX = (extent.width + 15) / 16;
    const u32 groupCountY = (extent.height + 15) / 16;
    cmd->Dispatch(groupCountX, groupCountY, 1);

    cmd->TransitionLayout(texture.get(), TextureLayout::ShaderReadOnly);
}
