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

    sceneShader = config.device->CreateShaderPath("Shaders/scene.spv");

    // Scene pipeline descriptor sets:
        // Set 0: Scene UBO (camera, lights, debug)
        // Set 1: Material (albedo, normal textures on a bindless array)
        // Set 2: Skybox cubemap (for IBL reflections)
        // Set 3: Instanced Data
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
        .vertexShader = sceneShader.get(),
        .fragmentShader = sceneShader.get(),
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


void SceneRenderer::PrepareFrame(const Platform::WindowContext* window, const Camera* camera, f32 aspectRatio)
{
    if (!window) return;

    standardBucket.clear();
    transparentBucket.clear();
    totalVisibleInstances = 0;

    // Resetting the sizes of the existing batches instead of clearing the whole bucket....
    for (auto& batch : instanceBucket) {
        batch.instanceData.clear();
    }

    // Calculate what the matrix SHOULD be for this frame
    const glm::mat4 newVP = camera->projection * camera->view;
    frustum.Update(newVP);

    // Sorting models to their respective buckets before presentation :3
    for (const auto& inst : *config.models)
    {
        if (!frustum.IsBoxInFrustum(inst.model->modelBounds, inst.transform)) continue;

        if (inst.model->materials[0].type == MaterialType::Transparent)
        {
            transparentBucket.push_back(&inst);
        }

        if (inst.path == Renderer::RenderPath::Instance)
        {
            // Find existing batch for this model
            InstanceBatch* targetBatch = nullptr;
            for (auto& batch : instanceBucket)
            {
                if (batch.model == inst.model)
                {
                    targetBatch = &batch;
                    break;
                }
            }

            // If no batch exists, create one
            if (!targetBatch)
            {
                instanceBucket.push_back({inst.model, {}});
                targetBatch = &instanceBucket.back();
            }

            // Add the instance data
            targetBatch->instanceData.push_back({
                .worldMatrix = inst.transform,
                .materialIndex = inst.materialIndex,
                .roughness = inst.roughness,
                .metallic = inst.metallic
            });
            totalVisibleInstances++; // add to the count
        }
        else
        {
            standardBucket.push_back(&inst);
        }
    }

    // std::ranges::sort(transparentBucket, [camera](const auto* a, const auto* b)
    // {
    //     // Use squared distance for performance (avoiding sqrt)
    //     f32 distA = glm::distance2(camera->position, glm::vec3(a->transform[3]));
    //     f32 distB = glm::distance2(camera->position, glm::vec3(b->transform[3]));
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


        PushConstants pc = {
            .model = worldMatrix,
            .normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix))),
            .vertexOffset = part.vertexOffset,
            .vertexBufferAddress = model->vertexBufferAddress,
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

void SceneRenderer::DrawInstancedBatch(Renderer::VulkanModel* model, u32 count, u32 offset, Renderer::DrawCache& dc)
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
            .vertexBufferAddress = model->vertexBufferAddress,
            .isInstanced = 1,
            .materialIndex = part.materialIndex
        };

        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, sizeof(PushConstants), &pc);
        dc.cmd->DrawIndexed(part.indexCount, count, part.firstIndex, static_cast<i32>(offset), 0);

        dc.stats->drawCallCount++;
        dc.stats->totalTris += (part.indexCount / 3) * count;
    }
}

void SceneRenderer::RenderModels(Renderer::GPUCommandBuffer* cmd, const u32 frameIndex, SceneStats& stats)
{
    stats.drawCallCount = 0;
    stats.totalTris = 0;
    stats.totalMeshCount = 0;

    if (!config.models || (standardBucket.empty() && transparentBucket.empty() && totalVisibleInstances == 0)) return;

    stats.totalMeshCount = static_cast<u32>(standardBucket.size() + transparentBucket.size() + totalVisibleInstances);

    const u32 resIdx = frameIndex % MAX_FRAME_OVERLAP;

    megaStagingData.clear();
    megaStagingData.resize(totalVisibleInstances);

    size_t offset = 0;
    for (const auto& batch : instanceBucket)
    {
        if (batch.instanceData.empty()) continue;

        const size_t batchSize = batch.instanceData.size();
        std::memcpy(megaStagingData.data() + offset, batch.instanceData.data(), batchSize * sizeof(GPUInstanceSSBO));
        offset += batchSize;
    }

    Renderer::VulkanBuffer currentIB; // Empty buffer to force first bind
    Renderer::DrawCache dc = {
        .activePipeline = opaquePipeline.get(),
        .cmd = cmd,
        .stats = &stats,
        .lastIndexBuffer = &currentIB,
        .frameIndex = resIdx
    };

    if (!megaStagingData.empty())
    {
        instanceBuffer->UpdateBinding(resIdx, 0, megaStagingData.data(),
                                      megaStagingData.size() * sizeof(GPUInstanceSSBO));
    }

    auto BindGlobalSets = [&](Renderer::VulkanPipeline* pipeline)
    {
        cmd->BindPipeline(pipeline);
        dc.activePipeline = pipeline;

        // Set 0: Scene Data (Camera, Lights, Debug)
        cmd->BindDescriptorSet(&config.sceneUBO->descriptorSets[resIdx], 0, pipeline);

        // Set 2: Environment/Skybox
        if (config.skybox)
        {
            auto skySet = config.skybox->GetDescriptorSet();
            if (skySet) cmd->BindDescriptorSet(&skySet, 2, pipeline);
        }

        // Set 3: Instance Data SSBO
        instanceBuffer->Bind(cmd, *pipeline, resIdx);

        // Set 1 is NOT bound here because it is model-specific!
        dc.lastMaterialSet = {};
        dc.lastIndexBuffer = nullptr;
    };


    // OPAQUE PASS
    BindGlobalSets(opaquePipeline.get());
    for (const auto* inst : standardBucket) DrawStandardObject(inst, dc);

    // Draw Instanced Mega-Buffer
    u32 globalInstanceOffset = 0;
    for (const auto& batch : instanceBucket)
    {
        const u32 instanceCount = static_cast<u32>(batch.instanceData.size());
        if (instanceCount == 0) continue;

        if (config.debugRenderer && config.debugRenderer->enabled)
        {
            for (const auto& data : batch.instanceData)
            {
                config.debugRenderer->QueueBox(data.worldMatrix, batch.model->modelBounds);
            }
        }

        // Pass the starting index in the SSBO to the draw call
        DrawInstancedBatch(batch.model, instanceCount, globalInstanceOffset, dc);

        globalInstanceOffset += instanceCount;
    }

    if (!transparentBucket.empty())
    {
        BindGlobalSets(transparentPipeline.get());
        for (const auto* inst : transparentBucket) DrawStandardObject(inst, dc);
    }
}
