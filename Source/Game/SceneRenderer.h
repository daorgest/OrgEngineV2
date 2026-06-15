//
// Created by Orgest on 11/4/2025.
//

#pragma once
#include "AABB.h"
#include "BindlessManager.h"
#include "MeshStats.h"
#include "RenderInterface.h"
#include "VulkanMesh.h"

struct Camera;

class DebugRenderer;
class SkyboxManager;

struct SceneRenderConfig
{
    Renderer::GPUDevice* device = nullptr;
	Renderer::GPUSwapchain* swapchain = nullptr;
    Renderer::DescriptorAllocatorGrowable* descriptorAllocator = nullptr;
    Renderer::BindlessManager* bindless = nullptr;
    Renderer::GPUShaderManager* shaderManager = nullptr;
    AssetPool<Renderer::GPUModel>* modelPool = nullptr;

    Renderer::GPUShaderBuffer* sceneUBO = nullptr;
    SkyboxManager* skybox = nullptr;
    DebugRenderer* debugRenderer = nullptr;

    const Vector<ResourceHandle<Renderer::GPUModel>>* entityModels = nullptr;
    const Vector<Renderer::TransformComponent>* entityTransforms = nullptr;
    const Vector<Renderer::RenderPathComponent>* entityPaths = nullptr;
    const Vector<Renderer::MaterialComponent>* entityMaterials = nullptr;
};

class SceneRenderer
{
public:
    void Init(const SceneRenderConfig& cfg);
    void CreatePipelines();
    void PrepareFrame(const Platform::WindowContext* window, const Camera* camera);
    void RenderModels(Renderer::GPUCommandBuffer* cmd, u32 frameIndex, SceneStats& stats);
    void UpdateInstanceBuffer();
    void DestroyBuffers();
    void Destroy();

    Renderer::GPUPipeline* GetOpaquePipeline() const { return opaquePipeline.get(); }
    Renderer::GPUPipeline* GetTransparentPipeline() const { return transparentPipeline.get(); }
private:
    size_t totalVisibleInstances = 0;
    size_t totalFlattenedEntries = 0;
    u32 frameIndex = 0;

    // --- State & Buckets ---
    struct InstanceBatch {
        Renderer::GPUModel* model = nullptr;
        Vector<Engine::GPUInstanceSSBO> instanceData;
    };

    static InstanceBatch* GetOrAddBatch(Vector<InstanceBatch>& bucket, Renderer::GPUModel* model);
    void BindGlobalState(Renderer::DrawCache& dc, Renderer::GPUPipeline* pipeline) const;
    void RenderInstancedPass(Span<InstanceBatch> batches, Renderer::DrawCache& dc, bool isTransparentPass);
    void RenderIndirectPass(Span<InstanceBatch> batches, Renderer::GPUBuffer* buffer, Renderer::DrawCache& dc, bool isTransparentPass);

    void DrawObject(size_t i, Renderer::DrawCache& dc, bool isTransparentPass) const;
    void DrawInstancedBatch(Renderer::GPUModel* model, u32 count, u32 globalInstanceOffset, Renderer::DrawCache& dc, bool isTransparentPass);

    Vector<size_t> standardBucket;
    Vector<InstanceBatch> instanceBucket;
    Vector<InstanceBatch> indirectBucket;

    // Split Command Buffers
    Vector<Engine::GPUIndirectCommand> opaqueIndirectCommands;
    Vector<Engine::GPUIndirectCommand> transparentIndirectCommands;
    Vector<Engine::GPUInstanceSSBO> megaStagingData;

    std::unique_ptr<Renderer::GPUShaderBuffer> instanceBuffer;
    std::unique_ptr<Renderer::GPUShaderBuffer> materialBuffer;

    Array<std::unique_ptr<Renderer::GPUBuffer>, MAX_FRAME_OVERLAP> opaqueIndirectBuffers;
    Array<std::unique_ptr<Renderer::GPUBuffer>, MAX_FRAME_OVERLAP> transparentIndirectBuffers;

    Frustum frustum;
    SceneRenderConfig config = {};
    std::shared_ptr<Renderer::GPUShader> sceneShader;
    std::unique_ptr<Renderer::GPUPipeline> opaquePipeline;
    std::unique_ptr<Renderer::GPUPipeline> transparentPipeline;
};

