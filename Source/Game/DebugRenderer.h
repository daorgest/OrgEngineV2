#pragma once

#include "VulkanPipeline.h"
#include "VulkanShader.h"
#include "VulkanShaderBuffer.h"
#include <glm/glm.hpp>

struct BBoxPush
{
	glm::mat4 model;
	glm::vec3 aabbMin;
	float depthBias;
	glm::vec3 aabbMax;
	u32 flags;
	glm::vec4 color;
};

struct ArenaAllocator;

class DebugRenderer
{
public:
	bool Initialize(Renderer::VulkanDevice* dev, ArenaAllocator* arena, Renderer::VulkanShaderBuffer* sceneUBO, Renderer::DescriptorAllocatorGrowable* globalDescriptorAllocator, bool depthTest = true, bool alwaysOnTop = false);
	void QueueBox(const glm::mat4& model, const glm::vec3& min, const glm::vec3& max);
	void Flush(VkCommandBuffer cmd, u32 frameIndex);
	void ClearQueue() { drawQueue.clear(); }
	void Cleanup();

	// Settings
	void SetColor(const glm::vec4& col) { color = col; }
	void SetDepthBias(float bias) { depthBias = bias; }
	void SetFlags(u32 f) { flags = f; }

	Renderer::VulkanDevice* device = nullptr;
	Renderer::VulkanShader* shader = nullptr;
	Renderer::VulkanShaderBuffer* instanceBuffer = nullptr;    // arena-owned
	Renderer::VulkanShaderBuffer* sceneUBO = nullptr;          // external
	Renderer::VulkanPipeline pipeline;

	u32 maxInstances = 1000;
	glm::vec4 color = {1.0f, 1.0f, 0.0f, 1.0f};
	float depthBias = 0.0f;
	u32 flags = 0;
	bool enabled = false;

	Vector<BBoxPush> drawQueue;
};