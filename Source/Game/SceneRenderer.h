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
	Renderer::GPUShader* vertexShader = nullptr;
	Renderer::GPUShader* fragmentShader = nullptr;
	Renderer::DescriptorAllocatorGrowable* descriptorAllocator;
	Renderer::VulkanShaderBuffer* sceneUBO = nullptr;
	SkyboxManager* skybox = nullptr;
	DebugRenderer* debugRenderer = nullptr;

	Vector<Renderer::ModelComponent>* models = nullptr;
	u32 drawLimit = 100000;
};

class SceneRenderer
{
public:
	SceneRenderer() = default;

	void Init(SceneRenderConfig& cfg);
	void PrepareFrame(const Platform::WindowContext* window, const Camera* camera, bool freeze);
	void DrawStandardObject(const Renderer::ModelComponent* inst,
	                        Renderer::DrawCache& dc) const;
	void DrawInstancedBatch(
		Renderer::VulkanModel* model, u32 count, u32 offset, Renderer::DrawCache& dc) const;


	void RenderModels(Renderer::GPUCommandBuffer* cmd, u32 frameIndex, SceneStats& stats) const;

private:
	struct InstanceBatch {
		Renderer::VulkanModel* model;
		Vector<GPUInstanceSSBO> instanceData;
	};

	Vector<const Renderer::ModelComponent*> standardBucket;
	Vector<const Renderer::ModelComponent*> transparentBucket;
	std::unordered_map<Renderer::VulkanModel*, InstanceBatch> instanceBucket;
	std::unique_ptr<Renderer::VulkanShaderBuffer> instanceBuffer;

	std::unique_ptr<Renderer::VulkanShaderBuffer> materialBuffer;

	Frustum frustum;
	SceneRenderConfig config;
	std::unique_ptr<Renderer::VulkanPipeline> opaquePipeline;
	std::unique_ptr<Renderer::VulkanPipeline> transparentPipeline;
};

