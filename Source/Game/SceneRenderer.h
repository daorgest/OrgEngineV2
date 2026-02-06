//
// Created by Orgest on 11/4/2025.
//

#pragma once
#include "AABB.h"
#include "Platform.h"
#include "RenderInterface.h"
#include "VulkanMesh.h"
#include "VulkanPipeline.h"
#include "VulkanShaderBuffer.h"

struct Camera;
struct GPUInstanceSSBO;

namespace Renderer
{
	struct VulkanModel;
	struct ModelComponent;
}

class DebugRenderer;
class SkyboxManager;

struct SceneRenderConfig
{
	Renderer::VulkanDevice* device = nullptr; // will abstract soon, but vulkan pipeline depends on it
	Renderer::DescriptorAllocatorGrowable* descriptorAllocator;
	Renderer::VulkanShaderBuffer* sceneUBO = nullptr;
	SkyboxManager* skybox = nullptr;
	DebugRenderer* debugRenderer = nullptr;

	std::span<Renderer::ModelComponent> models;
	u32 drawLimit = 100000;
};

class SceneRenderer
{
public:
	SceneRenderer() = default;

	void Init(SceneRenderConfig& cfg);
	void PrepareFrame(const Platform::WindowContext* window, const Camera* camera);
	void DrawStandardObject(const Renderer::ModelComponent* inst, Renderer::DrawCache& dc) const;
    static void DrawInstancedBatch(Renderer::VulkanModel* model, u32 count, u32 offset, Renderer::DrawCache& dc);
	void RenderModels(Renderer::GPUCommandBuffer* cmd, u32 frameIndex, SceneStats& stats);

private:
	struct InstanceBatch {
		Renderer::VulkanModel* model;
		Vector<GPUInstanceSSBO> instanceData; // the models instance data. imagine 1 model of grass with multiple properties for each
	};

	Vector<const Renderer::ModelComponent*> standardBucket; // Opaque objects
	Vector<const Renderer::ModelComponent*> transparentBucket; // Transparent objects
    Vector<InstanceBatch> instanceBucket; // Visibles instances
    size_t totalVisibleInstances = 0; // Count of instances based on frustum culling test
    Vector<GPUInstanceSSBO> megaStagingData; // the massive buffer for instance data..... won't realloc unless the visible instance count grows
	std::unique_ptr<Renderer::VulkanShaderBuffer> instanceBuffer; // instance data for vertex shader
	std::unique_ptr<Renderer::VulkanShaderBuffer> materialBuffer; // material data for texture indices, material properties

	Frustum frustum;
	SceneRenderConfig config;

    std::unique_ptr<Renderer::GPUShader> sceneShader;

    // Pipelines for the render states
	std::unique_ptr<Renderer::VulkanPipeline> opaquePipeline;
	std::unique_ptr<Renderer::VulkanPipeline> transparentPipeline;
};

