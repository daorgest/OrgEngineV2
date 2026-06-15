//
// Created by Orgest on 11/4/2025.
//

#include "SceneRenderer.h"

#include <glm/gtx/norm.hpp>
#include <tracy/Tracy.hpp>
#include "DebugRenderer.h"
#include "SkyboxManager.h"
#include "../Engine/ShaderConstants.h"

using namespace Renderer;
using namespace Engine;

void SceneRenderer::Init(const SceneRenderConfig& cfg)
{
    config = cfg;

    sceneShader = config.device->CreateShaderPath("Shaders/scene.spv");

    CreatePipelines();

    const auto& [setLayouts, pushConstants] = opaquePipeline->GetLayoutDesc();

    materialBuffer = config.device->CreateShaderBuffer(config.descriptorAllocator, setLayouts[1]);
    instanceBuffer = config.device->CreateShaderBuffer(config.descriptorAllocator, setLayouts[4]);

    constexpr u64 maxDrawCommands = 10000;
    BufferInfo indirectInfo = {
        .size = maxDrawCommands * sizeof(GPUIndirectCommand),
        .heapType = GPUHeapType::Upload,
        .usage = GPUBufferFlag::Indirect | GPUBufferFlag::Storage,
    };

    for (u32 i = 0; i < MAX_FRAME_OVERLAP; ++i)
    {
        opaqueIndirectBuffers[i] = config.device->CreateBuffer(indirectInfo);
        transparentIndirectBuffers[i] = config.device->CreateBuffer(indirectInfo);
    }
}

void SceneRenderer::CreatePipelines()
{
    ZoneScopedN("SceneRenderer::RecreatePipelines");

    config.device->WaitIdle();

    const SampleCount currentMSAA = config.device->currentSamples;

    PipelineLayoutDesc sceneLayout;
    sceneLayout.setLayouts = {
        DescriptorSetLayoutDesc::FromConstants(0, Constants::Scene),
        DescriptorSetLayoutDesc::FromConstants(1, Constants::MaterialBuffer),
        DescriptorSetLayoutDesc::FromConstants(2, Constants::BindlessTextures2D),
        DescriptorSetLayoutDesc::FromConstants(3, Constants::Skybox),
        DescriptorSetLayoutDesc::FromConstants(4, Constants::InstanceData),
        // DescriptorSetLayoutDesc::FromConstants(5, Constants::BindlessTextures3D)
    };
    sceneLayout.pushConstants = {
        {
            .size = sizeof(PushConstants),
            .offset = 0,
            .stages = ShaderStage::Vertex | ShaderStage::Fragment
        }
    };

    Renderer::GraphicsPipelineDesc sceneDesc = {
        .vertexShader = sceneShader,
        .fragmentShader = sceneShader,
        .raster = GpuRasterDesc::Opaque3D(TextureFormat::BGRA8_SRGB, TextureFormat::D32_SFLOAT),
        .layout = sceneLayout,
        .slangSourcePath = "Source/Game/Shaders/scene.slang"
    };
    sceneDesc.raster.sampleCount = currentMSAA;

    opaquePipeline = config.device->CreateGraphicsPipeline(sceneDesc);
    config.shaderManager->RegisterPipeline(opaquePipeline.get());

    Renderer::GraphicsPipelineDesc transDesc = sceneDesc;
    transDesc.raster = GpuRasterDesc::Transparent(TextureFormat::BGRA8_SRGB, TextureFormat::D32_SFLOAT);

    transDesc.raster.sampleCount = currentMSAA;
    transparentPipeline = config.device->CreateGraphicsPipeline(transDesc);
    config.shaderManager->RegisterPipeline(transparentPipeline.get());
}

void SceneRenderer::PrepareFrame(const Platform::WindowContext* window, const Camera* camera)
{
    if (!window) return;
    standardBucket.clear();
    instanceBucket.clear();
    indirectBucket.clear();

    totalVisibleInstances = 0;
    totalFlattenedEntries = 0;

    if (config.debugRenderer) config.debugRenderer->ClearQueue();

    const glm::mat4 viewProg = camera->projection * camera->view;
    frustum.Update(viewProg);

    const size_t entityCount = config.entityModels->size();

    for (size_t i = 0; i < entityCount; i++)
    {
        auto modelRes = config.modelPool->Get((*config.entityModels)[i]);
        if (!modelRes) continue;
        Renderer::GPUModel* model = *modelRes;

        const auto& transform = (*config.entityTransforms)[i];
        const auto& pathComp = (*config.entityPaths)[i];
        const auto& matComp = (*config.entityMaterials)[i];

        bool modelInFrustrum = frustum.IsBoxInFrustum(model->aabb, transform.worldMatrix);

        GPUInstanceSSBO instData = {
            .worldMatrix = transform.worldMatrix,
            .materialIndex = matComp.materialIndex,
            .roughness = matComp.roughness,
            .metallic = matComp.metallic
        };

        if (config.debugRenderer && config.debugRenderer->enabled)
        {
            // Model Frustrum
            if (frustum.IsBoxInFrustum(model->aabb, transform.worldMatrix))
            {
                // Model Bounds
                config.debugRenderer->QueueBox(transform.worldMatrix, model->aabb,
                                               glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

                for (const auto& part : model->parts)
                {
                    if (part.indexCount == 0) continue;

                    const glm::mat4 worldTransform = (part.localTransform == glm::mat4(1.0f))
                                                         ? transform.worldMatrix
                                                         : (transform.worldMatrix * part.localTransform);

                    // Part Frustrum
                    if (frustum.IsBoxInFrustum(part.aabb, worldTransform))
                    {
                        // Passed: Highlight Green
                        config.debugRenderer->QueueBox(worldTransform, part.aabb, glm::vec4(0.0f, 1.0f, 0.0f, 0.6f));
                    }
                    else
                    {
                        // Culled: Highlight Yellow
                        config.debugRenderer->QueueBox(worldTransform, part.aabb, glm::vec4(1.0f, 1.0f, 0.0f, 0.6f));
                    }
                }
            }
        }


        else
        {
            config.debugRenderer->QueueBox(transform.worldMatrix, model->aabb,
                                           glm::vec4(1.0f, 1.0f, 0.0f, 0.6f));
        }

        if (!modelInFrustrum)
        {
            continue;
        }

        if (pathComp.path == RenderPath::Instance)
        {
            GetOrAddBatch(instanceBucket, model)->instanceData.push_back(instData);
            totalVisibleInstances++;
        }
        else if (pathComp.path == RenderPath::Indirect)
        {
            GetOrAddBatch(indirectBucket, model)->instanceData.push_back(instData);
            totalFlattenedEntries += model->parts.size();
        }
        else
        {
            standardBucket.push_back(i);
        }
    }
}


void SceneRenderer::RenderModels(Renderer::GPUCommandBuffer* cmd, const u32 fIdx, SceneStats& stats)
{
    stats.ResetFrame();

    // Early out if nothing to draw
    if (standardBucket.empty() && instanceBucket.empty() && indirectBucket.empty()) return;

    frameIndex = fIdx % MAX_FRAME_OVERLAP;
    UpdateInstanceBuffer();

    Renderer::DrawCache dc = {.cmd = cmd, .stats = &stats};

    // ==========================================
    // 1. OPAQUE PASS
    // ==========================================
    cmd->BeginDebugLabel("Opaque Pass", 0.4f, 0.4f, 0.9f);
    BindGlobalState(dc, opaquePipeline.get());

    for (const auto entityIdx : standardBucket)
        DrawObject(entityIdx, dc, false);

    RenderInstancedPass(instanceBucket, dc, false);

    if (!opaqueIndirectCommands.empty())
    {
        RenderIndirectPass(indirectBucket, opaqueIndirectBuffers[fIdx].get(), dc, false);
    }
    cmd->EndDebugLabel();

    // ==========================================
    // 2. TRANSPARENT PASS
    // ==========================================
    cmd->BeginDebugLabel("Transparent Pass", 0.9f, 0.4f, 0.4f);
    BindGlobalState(dc, transparentPipeline.get());

    for (const auto entityIdx : standardBucket)
        DrawObject(entityIdx, dc, true);

    RenderInstancedPass(instanceBucket, dc, true);

    if (!transparentIndirectCommands.empty())
    {
        RenderIndirectPass(indirectBucket, transparentIndirectBuffers[fIdx].get(), dc, true);
    }
    cmd->EndDebugLabel();
}


void SceneRenderer::UpdateInstanceBuffer()
{
    ZoneScopedN("UpdateInstanceBuffer");
    size_t requiredSize = totalVisibleInstances + totalFlattenedEntries;
    if (requiredSize == 0) return;

    megaStagingData.clear();
    megaStagingData.reserve(requiredSize);

    opaqueIndirectCommands.clear();
    transparentIndirectCommands.clear();

    for (const auto& batch : instanceBucket) {
        for (const auto& inst : batch.instanceData) {
            megaStagingData.push_back(inst);
        }
    }

    for (const auto& batch : indirectBucket)
    {
        if (batch.instanceData.empty()) continue;

        for (const auto& part : batch.model->parts)
        {
            if (part.indexCount == 0) continue;

            const bool isTransparent = (batch.model->materials[part.materialIndex].materialType ==
                Engine::MaterialType::Transparent);

            u32 visibleInstanceCount = 0;
            u32 firstInstanceOffset = static_cast<u32>(megaStagingData.size());

            // Flatten the SSBO data and check Frustum PER INSTANCE of this part
            for (const auto& inst : batch.instanceData)
            {
                const glm::mat4 worldTransform = (part.localTransform == glm::mat4(1.0f))
                                                     ? inst.worldMatrix
                                                     : (inst.worldMatrix * part.localTransform);

                if (frustum.IsBoxInFrustum(part.aabb, worldTransform))
                {
                    GPUInstanceSSBO flattened = {
                        .worldMatrix = worldTransform,
                        .materialIndex = part.materialIndex,
                        .roughness = inst.roughness,
                        .metallic = inst.metallic
                    };
                    megaStagingData.push_back(flattened);
                    visibleInstanceCount++;
                }
            }

            if (visibleInstanceCount > 0)
            {
                Engine::GPUIndirectCommand cmd = {
                    .indexCount = part.indexCount,
                    .instanceCount = visibleInstanceCount,
                    .firstIndex = part.firstIndex,
                    .vertexOffset = static_cast<i32>(part.vertexOffset),
                    .firstInstance = firstInstanceOffset
                };

                if (isTransparent) transparentIndirectCommands.push_back(cmd);
                else opaqueIndirectCommands.push_back(cmd);
            }
        }
    }

    if (!megaStagingData.empty())
    {
        instanceBuffer->UpdateBinding(frameIndex, 0, megaStagingData.data(),
                                      megaStagingData.size() * sizeof(Engine::GPUInstanceSSBO));
    }
    if (!opaqueIndirectCommands.empty())
    {
        opaqueIndirectBuffers[frameIndex]->Upload(opaqueIndirectCommands.data(),
                                                  opaqueIndirectCommands.size() * sizeof(Engine::GPUIndirectCommand));
    }
    if (!transparentIndirectCommands.empty())
    {
        transparentIndirectBuffers[frameIndex]->Upload(transparentIndirectCommands.data(),
                                                       transparentIndirectCommands.size() * sizeof(
                                                           Engine::GPUIndirectCommand));
    }
}

void SceneRenderer::DestroyBuffers()
{
    if (materialBuffer) materialBuffer->Destroy();
    if (instanceBuffer) instanceBuffer->Destroy();

    // Clear unique_ptrs
    materialBuffer.reset();
    instanceBuffer.reset();
}

void SceneRenderer::Destroy()
{
    if (config.device)
    {
        config.device->WaitIdle();
    }

    // Explicitly destroy buffers
    if (materialBuffer) materialBuffer->Destroy();
    if (instanceBuffer) instanceBuffer->Destroy();

    // Destroy pipelines
    opaquePipeline.reset();
    transparentPipeline.reset();
}

SceneRenderer::InstanceBatch* SceneRenderer::GetOrAddBatch(Vector<InstanceBatch>& bucket, Renderer::GPUModel* model)
{
    if (!bucket.empty() && bucket.back().model == model) return &bucket.back();


    for (auto& batch : bucket)
    {
        if (batch.model == model) return &batch;
    }

    bucket.push_back({model, {}});
    return &bucket.back();
}

void SceneRenderer::BindGlobalState(Renderer::DrawCache& dc, Renderer::GPUPipeline* pipeline) const
{
    ZoneScopedN("SceneRenderer::BindGlobalState");

    dc.Flush();

    dc.cmd->BindPipeline(pipeline);
    dc.activePipeline = pipeline;

    // Set 0: Scene Globals (UBO)
    config.sceneUBO->Bind(dc.cmd, pipeline, frameIndex);

    // Set 2: Bindless 2D Textures
    dc.cmd->BindDescriptorSet(&config.bindless->set, 2, pipeline);

    // // Set 5: Bindless 3D Textures
    // dc.cmd->BindDescriptorSet(&config.bindless->set3D, 5, pipeline);

    // Set 3: Environment & Skybox (IBL)
    if (config.skybox)
    {
        dc.cmd->BindDescriptorSet(&config.skybox->GetDescriptorSet(), 3, pipeline);
    }

    // Set 4: Pass-wide Instance Data (The Flattened SSBO)
    if (instanceBuffer)
    {
        instanceBuffer->Bind(dc.cmd, pipeline, frameIndex);
    }
}

void SceneRenderer::RenderInstancedPass(const Span<InstanceBatch> batches, Renderer::DrawCache& dc, const bool isTransparentPass)
{
    u32 globalInstanceOffset = 0;
    for (const auto& batch : batches)
    {
        const u32 count = static_cast<u32>(batch.instanceData.size());
        if (count == 0) continue;

        DrawInstancedBatch(batch.model, count, globalInstanceOffset, dc, isTransparentPass);

        globalInstanceOffset += count;
    }
}

void SceneRenderer::RenderIndirectPass(const Span<InstanceBatch> batches, Renderer::GPUBuffer* buffer,
                                       Renderer::DrawCache& dc, const bool isTransparentPass)
{
    u32 currentCommandIndex = 0;

    for (const auto& batch : batches)
    {
        const u32 instanceCount = static_cast<u32>(batch.instanceData.size());
        if (instanceCount == 0) continue;

        Renderer::GPUModel* model = batch.model;
        if (!model || !model->indexBuffer || model->vertexBufferAddress == 0) continue;

        if (model->materialBuffer) model->materialBuffer->Bind(dc.cmd, dc.activePipeline, frameIndex);

        if (model->indexBuffer.get() != dc.lastIndexBuffer)
        {
            dc.cmd->BindIndexBuffer(model->indexBuffer.get(), 0);
            dc.lastIndexBuffer = model->indexBuffer.get();
        }

        // Count valid parts for this pass
        u32 validPartsForThisPass = 0;
        for (const auto& part : model->parts)
        {
            const bool partIsTrans = (model->materials[part.materialIndex].materialType ==
                Engine::MaterialType::Transparent);
            if (partIsTrans == isTransparentPass) validPartsForThisPass++;
        }

        if (validPartsForThisPass > 0)
        {
            PushConstants pc = {
                .vertexBufferAddress = model->vertexBufferAddress,
                .renderPath = RenderPath::Indirect,
            };

            const u64 bufferMemoryOffset = currentCommandIndex * sizeof(Engine::GPUIndirectCommand);

            dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, sizeof(PushConstants), &pc);
            dc.cmd->DrawIndexedIndirect(buffer, bufferMemoryOffset, validPartsForThisPass,
                                        sizeof(Engine::GPUIndirectCommand));

            dc.stats->drawCallCount++;
            for (const auto& p : model->parts) dc.stats->totalTris += (p.indexCount / 3) * instanceCount;

            currentCommandIndex += validPartsForThisPass;
        }
    }
}

void SceneRenderer::DrawObject(const size_t i, Renderer::DrawCache& dc, const bool isTransparentPass) const
{
    const auto& modelRes = config.modelPool->Get((*config.entityModels)[i]);
    if (!modelRes) return;
    Renderer::GPUModel* model = *modelRes;

    if (!model->indexBuffer || !model->indexBuffer->IsValid()) return;

    if (model->materialBuffer)
    {
        model->materialBuffer->Bind(dc.cmd, dc.activePipeline, frameIndex);
    }

    if (model->indexBuffer.get() != dc.lastIndexBuffer)
    {
        dc.cmd->BindIndexBuffer(model->indexBuffer.get(), 0);
        dc.lastIndexBuffer = model->indexBuffer.get();
    }

    const auto& transform = (*config.entityTransforms)[i];
    const auto& matComp = (*config.entityMaterials)[i];

    for (const auto& part : model->parts)
    {
        const bool partIsTrans = (model->materials[part.materialIndex].materialType ==
            Engine::MaterialType::Transparent);
        if (partIsTrans != isTransparentPass) continue;

        const glm::mat4& worldMatrix = (part.localTransform == glm::mat4(1.0f))
                                           ? transform.worldMatrix
                                           : (transform.worldMatrix * part.localTransform);

        if (!frustum.IsBoxInFrustum(part.aabb, worldMatrix)) continue;

        PushConstants pc = {
            .model = worldMatrix,
            .vertexBufferAddress = model->vertexBufferAddress,
            .vertexOffset = part.vertexOffset,
            .renderPath = RenderPath::Standard,
            .instRoughness = matComp.roughness,
            .instMetallic = matComp.metallic,
            .materialIndex = part.materialIndex
        };

        const u32 pushSize = dc.activePipeline->GetLayoutDesc().pushConstants[0].size;
        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, pushSize, &pc);
        dc.cmd->DrawIndexed(part.indexCount, 1, part.firstIndex, 0, 0);

        dc.stats->drawCallCount++;
        dc.stats->totalTris += (part.indexCount / 3);
    }
}

void SceneRenderer::DrawInstancedBatch(Renderer::GPUModel* model, const u32 count, const u32 globalInstanceOffset,
                                       Renderer::DrawCache& dc, const bool isTransparentPass)
{
    if (!model || !model->indexBuffer || model->vertexBufferAddress == 0) return;

    if (model->materialBuffer)
    {
        model->materialBuffer->Bind(dc.cmd, dc.activePipeline, frameIndex);
    }

    if (model->indexBuffer.get() != dc.lastIndexBuffer)
    {
        dc.cmd->BindIndexBuffer(model->indexBuffer.get(), 0);
        dc.lastIndexBuffer = model->indexBuffer.get();
    }

    for (const auto& part : model->parts)
    {
        const bool partIsTrans = (model->materials[part.materialIndex].materialType == Engine::MaterialType::Transparent);
        if (partIsTrans != isTransparentPass) continue;

        PushConstants pc = {
            .model = part.localTransform,
            .vertexBufferAddress = model->vertexBufferAddress,
            .renderPath = RenderPath::Instance,
            .materialIndex = part.materialIndex
        };

        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, sizeof(PushConstants), &pc);
        dc.cmd->DrawIndexed(part.indexCount, count, part.firstIndex, part.vertexOffset, globalInstanceOffset);

        dc.stats->drawCallCount++;
        dc.stats->totalTris += (part.indexCount / 3) * count;
    }
}
