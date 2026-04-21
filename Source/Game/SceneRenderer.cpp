//
// Created by Orgest on 11/4/2025.
//

#include "SceneRenderer.h"
#include "ShaderCompiler.h"
#include <ranges>

#include "Application.h"
#include "DebugRenderer.h"
#include "../Engine/ShaderConstants.h"
#include "SkyboxManager.h"
#include "glm/gtx/norm.hpp"
#include "tracy/Tracy.hpp"

void SceneRenderer::Init(SceneRenderConfig& cfg)
{
    config = cfg;

    sceneShader = config.device->CreateShaderPath("Shaders/scene.spv");

    PipelineLayoutDesc sceneLayout;
    sceneLayout.setLayouts = {
                { .setIndex = 0, .bindings = Constants::Scene },
                { .setIndex = 1, .bindings = Constants::MaterialBuffer },
                { .setIndex = 2, .bindings = Constants::BindlessTextures },
                { .setIndex = 3, .bindings = Constants::Skybox },
                { .setIndex = 4, .bindings = Constants::InstanceData }
    };
    sceneLayout.pushConstants = {{
        .size = sizeof(PushConstants),
        .offset = 0,
        .stages = ShaderStage::Vertex | ShaderStage::Fragment
    }};

    // Opaque/Masked Pipeline
    const Renderer::GraphicsPipelineDesc sceneDesc = {
        .vertexShader = sceneShader,
        .fragmentShader = sceneShader,
        .raster = GpuRasterDesc::Opaque3D(TextureFormat::BGRA8_SRGB, TextureFormat::D32_SFLOAT),
        .layout = sceneLayout,
        .slangSourcePath = "Source/Game/Shaders/scene.slang"
    };

    opaquePipeline = config.device->CreateGraphicsPipeline(sceneDesc);
    cfg.shaderManager->RegisterPipeline(opaquePipeline.get());

    // Transparent Pipeline
    Renderer::GraphicsPipelineDesc transDesc = sceneDesc;
    transDesc.raster = GpuRasterDesc::Transparent(TextureFormat::BGRA8_SRGB, TextureFormat::D32_SFLOAT);
    transparentPipeline = config.device->CreateGraphicsPipeline(transDesc);
    cfg.shaderManager->RegisterPipeline(transparentPipeline.get());

    // Material information and instance buffer
    materialBuffer = config.device->CreateShaderBuffer(cfg.descriptorAllocator, sceneLayout.setLayouts[1]);
    instanceBuffer = config.device->CreateShaderBuffer(cfg.descriptorAllocator, sceneLayout.setLayouts[4]);

    // Indirect buffer (broken for now)
    constexpr u64 maxDrawCommands = 10000;
    indirectBuffer = config.device->CreateBuffer(BufferPreset::IndirectHost,maxDrawCommands * sizeof(GPUIndirectCommand));
}


void SceneRenderer::PrepareFrame(const Platform::WindowContext* window, const Camera* camera)
{
    if (!window) return;
    standardBucket.clear();
    transparentBucket.clear();
    totalVisibleInstances = 0;
    totalFlattenedEntries = 0;
    for (auto& batch : instanceBucket)
        batch.instanceData.clear();

    for (auto& batch : indirectBucket)
        batch.instanceData.clear();

    const glm::mat4 vp = camera->projection * camera->view;
    frustum.Update(vp);

    for (const auto& inst : config.models)
    {

        auto modelRes = config.modelPool->Get(inst.modelHandle);
        if (!modelRes) continue;
        Renderer::GPUModel* model = *modelRes;

        if (!frustum.IsBoxInFrustum(model->modelBounds, inst.transform)) continue;


        if (config.debugRenderer && config.debugRenderer->enabled)
        {
            config.debugRenderer->QueueBox(inst.transform, model->modelBounds);
        }

        if (inst.path == Renderer::RenderPath::Standard)
        {
            standardBucket.push_back(&inst);
        }
        else if (inst.path == Renderer::RenderPath::Instance)
        {
            auto targetBatch = GetOrAddBatch(instanceBucket, model);
            targetBatch->instanceData.push_back({
                inst.transform,
                inst.materialIndex,
                inst.roughness,
                inst.metallic
            });
            totalVisibleInstances++;
        }
        else if (inst.path == Renderer::RenderPath::Indirect)
        {
            auto targetBatch = GetOrAddBatch(indirectBucket, model);
            targetBatch->instanceData.push_back({
                inst.transform,
                inst.materialIndex,
                inst.roughness,
                inst.metallic
            });
            totalFlattenedEntries += model->parts.size();
        }
    }
}


void SceneRenderer::UpdateInstanceBuffer(u32 frameIndex)
{
    ZoneScopedN("UpdateInstanceBuffer")
    size_t requiredSize = totalVisibleInstances + totalFlattenedEntries;
    if (requiredSize == 0) return;

    megaStagingData.clear();
    megaStagingData.reserve(requiredSize);

    indirectCommands.clear();

    // 1. Pack 'Instance' path (Direct Draw)
    // One entry per instance. Material/Vertex offsets are pushed via C++ loop later.
    for (const auto& batch : instanceBucket) {
        if (batch.instanceData.empty()) continue;
        for (const auto& inst : batch.instanceData) {
            megaStagingData.push_back(inst);
        }
    }

    // 2. Pack 'Indirect' path (Multi-Draw Flattening)
    for (const auto& batch : indirectBucket) {
        if (batch.instanceData.empty()) continue;
        const u32 instanceCount = static_cast<u32>(batch.instanceData.size());

        for (const auto& part : batch.model->parts) {

            // Build the command for this specific part
            GPUIndirectCommand cmd = {};
            cmd.indexCount    = part.indexCount;
            cmd.instanceCount = instanceCount;
            cmd.firstIndex    = part.firstIndex;
            cmd.vertexOffset  = part.vertexOffset;
            cmd.firstInstance = static_cast<u32>(megaStagingData.size());

            indirectCommands.push_back(cmd);

            // Flatten all instances for this part, baking in the material ID
            for (const auto& inst : batch.instanceData) {
                GPUInstanceSSBO flattened = inst;
                flattened.worldMatrix = (part.localTransform == glm::mat4(1.0f))
                               ? inst.worldMatrix
                               : (inst.worldMatrix * part.localTransform);

                flattened.materialIndex = part.materialIndex;
                megaStagingData.push_back(flattened);
            }
        }
    }

    if (megaStagingData.empty()) return;

    // Upload
    instanceBuffer->UpdateBinding(frameIndex, 0, megaStagingData.data(), megaStagingData.size() * sizeof(GPUInstanceSSBO));
    if (!indirectCommands.empty()) {
        indirectBuffer->Upload(indirectCommands.data(), indirectCommands.size() * sizeof(GPUIndirectCommand));
    }
}

void SceneRenderer::BindGlobalState(Renderer::DrawCache& dc, Renderer::GPUPipeline* pipeline) const
{
    ZoneScopedN("SceneRenderer::BindGlobalState");

    dc.Flush();

    dc.cmd->BindPipeline(pipeline);
    dc.activePipeline = pipeline;

    // Set 0: Scene Globals (UBO)
    if (config.sceneUBO)
    {
        config.sceneUBO->Bind(dc.cmd, pipeline, dc.frameIndex);
    }

    // Set 1: Bindless Textures
    dc.cmd->BindDescriptorSet(&config.bindless->set, 2, pipeline);
    dc.lastMaterialSet = config.bindless->set.vk;

    // Set 2: Environment & Skybox (IBL)
    if (config.skybox)
    {
        const auto skySet = config.skybox->GetDescriptorSet();
        if (skySet)
        {
            dc.cmd->BindDescriptorSet(&skySet, 3, pipeline);
        }
    }

    // Set 4: Pass-wide Instance Data
    instanceBuffer->Bind(dc.cmd, pipeline, dc.frameIndex);
}

void SceneRenderer::RenderInstancedPass(Renderer::DrawCache& dc)
{
    u32 globalInstanceOffset = 0;
    for (const auto& batch : instanceBucket)
    {
        const u32 count = static_cast<u32>(batch.instanceData.size());
        if (count == 0) continue;

        DrawInstancedBatch(batch.model, count, globalInstanceOffset, dc);
        globalInstanceOffset += count;
    }
}

void SceneRenderer::RenderIndirectPass(Renderer::DrawCache& dc)
{
    u32 currentCommandIndex = 0;

    for (const auto& batch : indirectBucket)
    {
        const u32 instanceCount = static_cast<u32>(batch.instanceData.size());
        if (instanceCount == 0) continue;

        Renderer::GPUModel* model = batch.model;
        if (!model || !model->indexBuffer || model->vertexBufferAddress == 0) continue;

        // 1Bind Model Resources
        if (model->materialBuffer) model->materialBuffer->Bind(dc.cmd, dc.activePipeline, dc.frameIndex);
        if (model->indexBuffer.get() != dc.lastIndexBuffer) {
            dc.cmd->BindIndexBuffer(model->indexBuffer.get(), 0);
            dc.lastIndexBuffer = model->indexBuffer.get();
        }

        u32 partCount = static_cast<u32>(model->parts.size());
        u64 bufferMemoryOffset = currentCommandIndex * sizeof(GPUIndirectCommand);

        // Set Multi-Draw Constants
        PushConstants pc = {};
        pc.vertexBufferAddress = model->vertexBufferAddress;
        pc.isInstanced = 1;
        pc.materialIndex = 0;// Shader reads baked material from flattened SSBO
        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, sizeof(PushConstants), &pc);

        // THE MAGIC MULTI-DRAW
        dc.cmd->DrawIndexedIndirect(indirectBuffer.get(), bufferMemoryOffset, partCount, sizeof(GPUIndirectCommand));

        // Update stats and advance index
        dc.stats->drawCallCount++;
        for (const auto& p : model->parts) dc.stats->totalTris += (p.indexCount / 3) * instanceCount;

        currentCommandIndex += partCount; // Advance after the draw
    }
}

void SceneRenderer::DrawObject(const Renderer::ModelComponent* inst, const Renderer::DrawCache& dc) const
{
    // Resolve model from pool using the handle
    const auto modelRes = config.modelPool->Get(inst->modelHandle);
    if (!modelRes) return;
    Renderer::GPUModel* model = *modelRes;

    if (!model->indexBuffer || !model->indexBuffer->IsValid()) return;

    if (model->materialBuffer) {
        model->materialBuffer->Bind(dc.cmd, dc.activePipeline, dc.frameIndex);
    }


    dc.cmd->BindIndexBuffer(model->indexBuffer.get(), 0);

    for (const auto& part : model->parts)
    {
        const glm::mat4 worldMatrix = (part.localTransform == glm::mat4(1.0f))
                                          ? inst->transform
                                          : (inst->transform * part.localTransform);
        if (!frustum.IsBoxInFrustum(part.aabb, worldMatrix)) continue;

        PushConstants pc = {
            .model = worldMatrix,
            .vertexBufferAddress = model->vertexBufferAddress,
            .vertexOffset = part.vertexOffset,
            .isInstanced = 0,
            .instRoughness = inst->roughness,
            .instMetallic = inst->metallic,
            .materialIndex = part.materialIndex
        };

        const u32 pushSize = dc.activePipeline->GetLayoutDesc().pushConstants[0].size;

        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, pushSize, &pc);
        dc.cmd->DrawIndexed(part.indexCount, 1, part.firstIndex, 0, 0);

        dc.stats->drawCallCount++;
        dc.stats->totalTris += (part.indexCount / 3);
    }
}

void SceneRenderer::DrawInstancedBatch(Renderer::GPUModel* model, u32 count, u32 offset, Renderer::DrawCache& dc)
{
    if (!model || !model->indexBuffer || model->vertexBufferAddress == 0) return;

    if (model->materialBuffer)
    {
        model->materialBuffer->Bind(dc.cmd, dc.activePipeline, dc.frameIndex);
    }

    // Redundancy Guard: Index Buffer
    if (model->indexBuffer.get() != dc.lastIndexBuffer)
    {
        dc.cmd->BindIndexBuffer(model->indexBuffer.get(), 0);
        dc.lastIndexBuffer = model->indexBuffer.get();
    }

    // Draw Sub-Meshes
    for (const auto& part : model->parts)
    {
        PushConstants pc = {
            .vertexBufferAddress = model->vertexBufferAddress,
            .vertexOffset = part.vertexOffset,
            .isInstanced = 1,
            .materialIndex = part.materialIndex
        };

        dc.cmd->PushConstants(dc.activePipeline, ShaderStage::AllGraphics, 0, sizeof(PushConstants), &pc);
        dc.cmd->DrawIndexed(part.indexCount, count, part.firstIndex, 0, offset);

        dc.stats->drawCallCount++;
        dc.stats->totalTris += (part.indexCount / 3) * count;
    }
}

SceneRenderer::InstanceBatch* SceneRenderer::GetOrAddBatch(Vector<InstanceBatch>& bucket, Renderer::GPUModel* model)
{
    for (auto& batch : bucket)
    {
        if (batch.model == model)
            return &batch;
    }

    bucket.push_back({model, {}});
    return &bucket.back();
}

void SceneRenderer::RenderModels(Renderer::GPUCommandBuffer* cmd, const u32 frameIndex, SceneStats& stats)
{
    stats.ResetFrame();

    stats.totalMeshCount = 0;
    stats.totalTris = 0;
    stats.totalVerts = 0;

    // Early Out & Prep
    if (standardBucket.empty() && transparentBucket.empty() && totalVisibleInstances == 0 && totalFlattenedEntries == 0) return;

    const u32 resIdx = frameIndex % MAX_FRAME_OVERLAP;
    stats.totalMeshCount = static_cast<u32>(standardBucket.size() + transparentBucket.size() + totalVisibleInstances);

    UpdateInstanceBuffer(resIdx);

    Renderer::DrawCache dc = { .cmd = cmd, .stats = &stats, .frameIndex = resIdx };

    cmd->BeginDebugLabel("Opaque Pass", 0.4f, 0.4f, 0.9f);
    {
        BindGlobalState(dc, opaquePipeline.get());

        // Standard Geometry
        for (const auto* inst : standardBucket) DrawObject(inst, dc);


        RenderInstancedPass(dc);

        RenderIndirectPass(dc);
    }
    cmd->EndDebugLabel();

    if (!transparentBucket.empty())
    {
        cmd->BeginDebugLabel("Transparent Pass", 0.9f, 0.4f, 0.4f);
        BindGlobalState(dc, transparentPipeline.get());
        for (const auto* inst : transparentBucket) DrawObject(inst, dc);
        cmd->EndDebugLabel();
    }
}
