#pragma once
#include <glm/glm.hpp>

#include "AABB.h"
#include "RenderInterface.h"

class DebugRenderer
{
public:
	bool Initialize(Renderer::GPUDevice* dev, Renderer::GPUShaderBuffer* ubo, Renderer::DescriptorAllocatorGrowable* alloc, bool depthTest
                        = true, bool alwaysOnTop = false);
    void QueueBox(const glm::mat4& model, const AABB& aabb, const glm::vec4& boxColor);
    void QueueBox(const glm::mat4& model, const AABB& aabb) { QueueBox(model, aabb, color); }
	void Flush(Renderer::GPUCommandBuffer* cmd, u32 frameIndex);
	void ClearQueue() { drawQueue.clear(); }
	void Cleanup();

	// Settings
	void SetColor(const glm::vec4& col) { color = col; }
	void SetDepthBias(f32 bias) { depthBias = bias; }
	void SetFlags(u32 f) { flags = f; }



    Renderer::GPUDevice* device = nullptr;
    Renderer::GPUShaderBuffer* sceneUBO = nullptr;
    std::shared_ptr<Renderer::GPUShader> shader;
    std::unique_ptr<Renderer::GPUShaderBuffer> instanceBuffer;
	std::unique_ptr<Renderer::GPUPipeline> pipeline;

    // Getters
    Renderer::GPUPipeline* GetPipeline() const { return pipeline.get(); }

	u32 maxInstances = 500000;
	glm::vec4 color = {1.0f, 1.0f, 0.0f, 1.0f};
	f32 depthBias = 0.0f;
	u32 flags = 0;
	bool enabled = false;

	Vector<Engine::BBoxPush> drawQueue;
};
