//
// Created by Orgest on 11/4/2025.
//

#include "SceneRenderer.h"

#include <ranges>

#include "Application.h"
#include "DebugRenderer.h"
#include "../Engine/ShaderConstants.h"
#include "SkyboxManager.h"
#include "glm/gtx/norm.hpp"

void SceneRenderer::Init(SceneRenderConfig& cfg)
{
    config = cfg;

    // Scene pipeline descriptor sets:
    // Set 0: Scene UBO (camera, lights, debug)
    // Set 1: Material (albedo, normal textures)
    // Set 2: Skybox cubemap (for IBL reflections)
    DescriptorSetLayoutDesc sceneLayouts[] = {
        {.setIndex = 0, .bindings = Constants::Scene},
        {.setIndex = 1, .bindings = Constants::Material},
        {.setIndex = 2, .bindings = Constants::Skybox},
        {.setIndex = 3, .bindings = Constants::InstanceData}
    };

    PushConstantDesc scenePushConstants = {
        .size = sizeof(PushConstants),
        .offset = 0,
        .stages = ShaderStage::Vertex | ShaderStage::Fragment
    };

    // Opaque/Masked Pipeline
    const Renderer::GraphicsPipelineDesc sceneDesc = {
        .vertexShader = config.vertexShader,
        .fragmentShader = config.fragmentShader,
        .raster = GpuRasterDesc::Opaque3D(TextureFormat::BGRA8_SRGB, TextureFormat::D32_SFLOAT),
        .layout = {
            .setLayouts = std::span(sceneLayouts),
            .pushConstants = std::span(&scenePushConstants, 1)
        }
    };

    opaquePipeline = std::make_unique<Renderer::VulkanPipeline>();
    if (!opaquePipeline->CreateGraphicsPipeline(config.device, sceneDesc))
    {
        LOG(Error, "SceneRenderer: Failed to create Opaque Pipeline");
    }

    // Transparent Pipeline
    Renderer::GraphicsPipelineDesc transDesc = sceneDesc;
    transDesc.raster = GpuRasterDesc::Transparent(TextureFormat::BGRA8_SRGB, TextureFormat::D32_SFLOAT);
    transparentPipeline = std::make_unique<Renderer::VulkanPipeline>();
    if (!transparentPipeline->CreateGraphicsPipeline(config.device, transDesc))
    {
        LOG(Error, "SceneRenderer: Failed to create Transparent Pipeline");
    }

    materialBuffer = std::make_unique<Renderer::VulkanShaderBuffer>(cfg.device, cfg.descriptorAllocator, sceneLayouts[1]);
    materialBuffer->AllocateDescriptorSets(true, 1000);

    instanceBuffer = std::make_unique<Renderer::VulkanShaderBuffer>(cfg.device, cfg.descriptorAllocator, sceneLayouts[3]);
    instanceBuffer->AllocateDescriptorSets();
}


void SceneRenderer::PrepareFrame(const Platform::WindowContext* window, const Camera* camera, bool freeze)
{
    if (!window || !camera) return;

    standardBucket.clear();
    transparentBucket.clear();
    instanceBucket.clear();

    if (!freeze)
    {
        const float aspect = static_cast<float>(window->windowWidth) / static_cast<float>(window->windowHeight);
        frustum.Update(camera->GetViewProjectionMatrix(aspect));
    }

    // sorting models
    for (const auto& inst : *config.models)
    {
        if (!frustum.IsBoxInFrustum(inst.model->modelBounds, inst.transform)) continue;

        if (inst.model->materials[0].type == MaterialType::Transparent)
        {
            transparentBucket.push_back(&inst);
            // continue;
        }

        if (inst.path == Renderer::RenderPath::Instance)
        {
            auto& batch = instanceBucket[inst.model];
            batch.model = inst.model;

            batch.instanceData.push_back({
                .worldMatrix = inst.transform,
                .materialIndex = inst.materialIndex,
                .roughness = inst.roughness,
                .metallic = inst.metallic
            });
        }
        else
        {
            standardBucket.push_back(&inst);
        }
    }

    // std::ranges::sort(transparentBucket, [camera](const auto* a, const auto* b)
    // {
    //     // Use squared distance for performance (avoiding sqrt)
    //     float distA = glm::distance2(camera->position, glm::vec3(a->transform[3]));
    //     float distB = glm::distance2(camera->position, glm::vec3(b->transform[3]));
    //     return distA > distB; // Back-to-Front
    // });
}


void SceneRenderer::DrawStandardObject(const Renderer::ModelComponent* inst, Renderer::DrawCache& dc) const
{
    auto* model = inst->model;
    if (!model || !model->indexBuffer.IsValid() || model->vertexBufferAddress == 0) return;

    auto& materialSet = model->materialBuffer->descriptorSets[dc.frameIndex];
    if (materialSet.vk != dc.lastMaterialSet.vk)
    {
        dc.cmd->BindDescriptorSet(&materialSet, 1, dc.activePipeline);
        dc.lastMaterialSet = materialSet;
    }
    // 2. Skip redundant Index Buffer Binding
    if (model->indexBuffer.buffer != (dc.lastIndexBuffer ? dc.lastIndexBuffer->buffer : nullptr))
    {
        dc.cmd->BindIndexBuffer(&model->indexBuffer, 0);
        dc.lastIndexBuffer = &model->indexBuffer;
    }

    for (const auto& part : model->parts)
    {
        const glm::mat4 worldMatrix = (part.localTransform == glm::mat4(1.0f))
                                          ? inst->transform
                                          : (inst->transform * part.localTransform);

        // Frustum Check (Part Level)
        if (!frustum.IsBoxInFrustum(part.aabb, worldMatrix)) continue;

        const auto& mat = model->materials[part.materialIndex];

        PushConstants pc = {
            .model = worldMatrix,
            .normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix))),
            .vertexOffset = part.vertexOffset,
            .deviceAddress = model->vertexBufferAddress,
            .isInstanced = 0,
            .instRoughness = inst->roughness,
            .instMetallic = inst->metallic,
            .materialIndex = part.materialIndex
        };

        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, sizeof(PushConstants), &pc);
        dc.cmd->DrawIndexed(part.indexCount, 1, part.firstIndex, 0, 0);

        if (config.debugRenderer && config.debugRenderer->enabled)
        {
            config.debugRenderer->QueueBox(pc.model, part.aabb);
        }

        dc.stats->drawCallCount++;
        dc.stats->totalTris += (part.indexCount / 3);
    }
}

void SceneRenderer::DrawInstancedBatch(Renderer::VulkanModel* model, u32 count, u32 offset, Renderer::DrawCache& dc) const
{
    if (!model || !model->indexBuffer.IsValid() || model->vertexBufferAddress == 0) return;


    auto& materialSet = model->materialBuffer->descriptorSets[dc.frameIndex];
    if (materialSet.vk != dc.lastMaterialSet.vk)
    {
        dc.cmd->BindDescriptorSet(&materialSet, 1, dc.activePipeline);
        dc.lastMaterialSet = materialSet;
    }

    if (model->indexBuffer.buffer != (dc.lastIndexBuffer ? dc.lastIndexBuffer->buffer : VK_NULL_HANDLE))
    {
        dc.cmd->BindIndexBuffer(&model->indexBuffer, 0);
        dc.lastIndexBuffer = &model->indexBuffer;
    }

    for (const auto& part : model->parts)
    {
        PushConstants pc = {
            .vertexOffset = part.vertexOffset,
            .deviceAddress = model->vertexBufferAddress,
            .isInstanced = 1,
            .materialIndex = part.materialIndex
        };

        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, sizeof(PushConstants), &pc);
        dc.cmd->DrawIndexed(part.indexCount, count, part.firstIndex, 0, offset);

        dc.stats->drawCallCount++;
        dc.stats->totalTris += (part.indexCount / 3) * count;
    }
}

void SceneRenderer::RenderModels(Renderer::GPUCommandBuffer* cmd, u32 frameIndex, SceneStats& stats) const
{
    if (!config.models || config.models->empty()) return;

    u32 resIdx = frameIndex % MAX_FRAME_OVERLAP;

    Vector<GPUInstanceSSBO> megaStagingData;
    for (const auto& batch : instanceBucket | std::views::values)
    {
        megaStagingData.insert((vecSizeType)megaStagingData.end(), batch.instanceData.begin(),
                               (vecSizeType)batch.instanceData.end());
    }

    stats.drawCallCount = 0;
    stats.totalTris = 0;
    Renderer::VulkanBuffer currentIB; // Empty buffer to force first bind
    Renderer::DrawCache dc = {
        .activePipeline = opaquePipeline.get(),
        .cmd = cmd,
        .stats = &stats,
        .lastIndexBuffer = &currentIB,
        .frameIndex = resIdx
    };

    if (!megaStagingData.empty()) {
        instanceBuffer->UpdateBinding(resIdx, 0, megaStagingData.data(), megaStagingData.size() * sizeof(GPUInstanceSSBO));
    }

    auto BindGlobalSets = [&](Renderer::VulkanPipeline* pipeline) {
        cmd->BindPipeline(pipeline);
        dc.activePipeline = pipeline;

        // Set 0: Scene Data (Camera, Lights, Debug)
        cmd->BindDescriptorSet(&config.sceneUBO->descriptorSets[resIdx], 0, pipeline);

        // Set 2: Environment/Skybox
        if (config.skybox) {
            auto skySet = config.skybox->GetDescriptorSet();
            if (skySet) cmd->BindDescriptorSet(&skySet, 2, pipeline);
        }

        // Set 3: Instance Data SSBO
        instanceBuffer->Bind(cmd, *pipeline, resIdx);

        // Set 1 is NOT bound here because it is model-specific!
        dc.lastMaterialSet = { VK_NULL_HANDLE };
        dc.lastIndexBuffer = nullptr;
    };


    // OPAQUE PASS
    BindGlobalSets(opaquePipeline.get());
    for (const auto* inst : standardBucket) DrawStandardObject(inst, dc);

    // Draw Instanced Mega-Buffer
    u32 globalOffset = 0;
    for (auto& [model, batch] : instanceBucket) {
        DrawInstancedBatch(model, (u32)batch.instanceData.size(), globalOffset, dc);
        globalOffset += (u32)batch.instanceData.size();
    }

    if (!transparentBucket.empty()) {
        BindGlobalSets(transparentPipeline.get());
        for (const auto* inst : transparentBucket) DrawStandardObject(inst, dc);
    }
}
