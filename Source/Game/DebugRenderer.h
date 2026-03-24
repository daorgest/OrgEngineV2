#pragma once

#include "VulkanPipeline.h"
#include "VulkanShader.h"
#include "VulkanShaderBuffer.h"
#include <glm/glm.hpp>

#include "AABB.h"

struct BBoxPush
{
	glm::mat4 model;
	glm::vec3 aabbMin;
	f32 depthBias;
	glm::vec3 aabbMax;
	u32 flags;
	glm::vec4 color;
};
static_assert(sizeof(BBoxPush) == 112, "Unexpected padding in BBoxPush!");

class DebugRenderer
{
public:
	bool Initialize(Renderer::GPUDevice* dev, Renderer::VulkanShaderBuffer* sceneUBO, Renderer::DescriptorAllocatorGrowable* globalDescriptorAllocator, bool depthTest = true, bool alwaysOnTop = false);
	void QueueBox(const glm::mat4& model, const AABB& aabb);
	void Flush(Renderer::GPUCommandBuffer* cmd, u32 frameIndex);
	void ClearQueue() { drawQueue.clear(); }
	void Cleanup();

	// Settings
	void SetColor(const glm::vec4& col) { color = col; }
	void SetDepthBias(f32 bias) { depthBias = bias; }
	void SetFlags(u32 f) { flags = f; }

	std::unique_ptr<Renderer::GPUShader> shader;
	std::unique_ptr<Renderer::VulkanShaderBuffer> instanceBuffer;    // internal
	Renderer::VulkanShaderBuffer* sceneUBO = nullptr;          // external
	std::unique_ptr<Renderer::GPUPipeline> pipeline;

	u32 maxInstances = 500000;
	glm::vec4 color = {1.0f, 1.0f, 0.0f, 1.0f};
	f32 depthBias = 0.0f;
	u32 flags = 0;
	bool enabled = false;

	Vector<BBoxPush> drawQueue;
};