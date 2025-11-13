//
// Created by Orgest on 11/4/2025.
//

#pragma once
#include "../Engine/MeshStats.h"
#include "../Renderer/Vulkan//VulkanCommands.h"
#include "../Renderer/Vulkan//VulkanPipeline.h"
#include "../Renderer/Vulkan//VulkanShaderBuffer.h"
#include "../Renderer/Vulkan/VulkanMesh.h"

class DebugRenderer;
class SkyboxManager;

struct SceneRenderConfig
{
	Renderer::VulkanPipeline* scenePipeline = nullptr;
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

	void Init(const SceneRenderConfig& config);

	// Main high-level render call - abstracts all Vulkan details
	void RenderModels(VkCommandBuffer cmd, u32 frameIndex, SceneStats& stats) const;

private:
	SceneRenderConfig config;
};

