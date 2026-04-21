//
// Created by Orgest on 11/4/2025.
//

#pragma once
#include "AABB.h"
#include "BindlessManager.h"
#include "MeshStats.h"
#include "Platform.h"
#include "RenderInterface.h"
#include "VulkanMesh.h"

namespace Renderer
{
    struct ModelComponent;
}

struct Camera;
struct GPUInstanceSSBO;

class DebugRenderer;
class SkyboxManager;

struct SceneRenderConfig
{
    Renderer::GPUDevice* device = nullptr;
    Renderer::DescriptorAllocatorGrowable* descriptorAllocator = nullptr;
    Renderer::BindlessManager* bindless = nullptr;
    Renderer::GPUShaderManager* shaderManager = nullptr;
    AssetPool<Renderer::GPUModel>* modelPool = nullptr;

    Renderer::GPUShaderBuffer* sceneUBO = nullptr;
    SkyboxManager* skybox = nullptr;
    DebugRenderer* debugRenderer = nullptr;

    std::span<Renderer::ModelComponent> models;
};

class SceneRenderer
{
public:
    void Init(SceneRenderConfig& cfg);
    void PrepareFrame(const Platform::WindowContext* window, const Camera* camera);
    void RenderModels(Renderer::GPUCommandBuffer* cmd, u32 frameIndex, SceneStats& stats);

    // RHI-based Accessors
    Renderer::GPUPipeline* GetOpaquePipeline() const { return opaquePipeline.get(); }
    Renderer::GPUShaderBuffer* GetMaterialBuffer() const { return materialBuffer.get(); }
    Renderer::GPUShaderBuffer* GetInstanceBuffer() const { return instanceBuffer.get(); }


    void UpdateInstanceBuffer(u32 frameIndex);
private:
    size_t totalVisibleInstances = 0;
    size_t totalFlattenedEntries = 0;

    // --- State & Buckets ---
    struct InstanceBatch {
        Renderer::GPUModel* model = nullptr;
        Vector<GPUInstanceSSBO> instanceData;
    };

    void BindGlobalState(Renderer::DrawCache& dc, Renderer::GPUPipeline* pipeline) const;

    void RenderInstancedPass(Renderer::DrawCache& dc);
    void RenderIndirectPass(Renderer::DrawCache& dc); // NEW

    void DrawObject(const Renderer::ModelComponent* inst, const Renderer::DrawCache& dc) const;
    static void DrawInstancedBatch(Renderer::GPUModel* model, u32 count, u32 globalInstanceOffset, Renderer::DrawCache& dc);

    static InstanceBatch* GetOrAddBatch(Vector<InstanceBatch>& bucket, Renderer::GPUModel* model);

    Vector<const Renderer::ModelComponent*> standardBucket;
    Vector<const Renderer::ModelComponent*> transparentBucket;
    Vector<InstanceBatch> instanceBucket;
    Vector<InstanceBatch> indirectBucket;

    Vector<GPUIndirectCommand> indirectCommands;
    Vector<GPUInstanceSSBO> megaStagingData;

    std::unique_ptr<Renderer::GPUShaderBuffer> instanceBuffer;
    std::unique_ptr<Renderer::GPUShaderBuffer> materialBuffer;
    std::unique_ptr<Renderer::GPUBuffer> indirectBuffer;

    Frustum frustum;
    SceneRenderConfig config = {};
    std::shared_ptr<Renderer::GPUShader> sceneShader;
    std::unique_ptr<Renderer::GPUPipeline> opaquePipeline;
    std::unique_ptr<Renderer::GPUPipeline> transparentPipeline;
};

