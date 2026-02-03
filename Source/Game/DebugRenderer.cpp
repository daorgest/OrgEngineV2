#include "DebugRenderer.h"

#include "VulkanDevice.h"
#include "Tools/Logger.h"
#include "VulkanShader.h"
#include "Tools/FileManager.h"

using namespace Renderer;

bool DebugRenderer::Initialize(GPUDevice* device, VulkanShaderBuffer* sceneUBO,
				DescriptorAllocatorGrowable* globalDescriptorAllocator, bool depthTest, bool alwaysOnTop)
{
	this->sceneUBO = sceneUBO;
    drawQueue.reserve(maxInstances);

    shader = device->CreateShaderPath("Shaders/boundingBox.spv");

	static Binding instBindings[] = {
		{
			.binding = 0,
			.type = DescriptorType::StorageBuffer,
			.stageFlags = ShaderStage::AllGraphics, // Moved from Desc to Binding
			.size = maxInstances * sizeof(BBoxPush)
		}
	};

	// Shader Instance buffer!!
	DescriptorSetLayoutDesc instDesc = {
		.setIndex = 1,
		.bindings = instBindings
	};

    // Soon to be rhi
    auto* vkDev = static_cast<VulkanDevice*>(device);
	instanceBuffer = std::make_unique<VulkanShaderBuffer>(vkDev, globalDescriptorAllocator, instDesc);
	instanceBuffer->AllocateDescriptorSets();


	static DescriptorSetLayoutDesc setLayouts[] = {
		{ .setIndex = 0, .bindings = sceneUBO->desc.bindings },      // Shared scene data
		{ .setIndex = 1, .bindings = instanceBuffer->desc.bindings } // Instance-specific BBox data
	};

	auto depthOp = CompareOp::Always;
	bool depthWrite   = false;

	if (!alwaysOnTop && depthTest) {
		depthOp    = CompareOp::Greater;
		depthWrite = true;
	}

	const GraphicsPipelineDesc debugDesc = {
		.vertexShader   = shader.get(),
		.fragmentShader = shader.get(),
		.raster = {
			.topology     = PrimitiveTopology::LineList,
			.cull         = CullMode::None,
			.depthFormat  = TextureFormat::D32_SFLOAT,
			.depthWrite   = depthWrite,
			.depthOp      = depthOp,
			.colorFormats = { TextureFormat::BGRA8_SRGB }
		},
		.layout = {
			.setLayouts = std::span(setLayouts)
		}
	};

	if (!pipeline.CreateGraphicsPipeline(vkDev, debugDesc))
	{
		LOG(Error, "Failed to create AABB Debug Pipeline");
		return false;
	}

	LOG(Info, "AABB Debug Pipeline created");
	return true;
}

void DebugRenderer::QueueBox(const glm::mat4& model, const AABB& aabb)
{
	drawQueue.push_back({model, aabb.Min(), depthBias, aabb.Max(), flags, color});
}

void DebugRenderer::Flush(GPUCommandBuffer* cmd, u32 frameIndex)
{
	if (drawQueue.empty())
		return;

	const size_t size = drawQueue.size() * sizeof(BBoxPush);

	// Upload all instance data
	instanceBuffer->Update(frameIndex, drawQueue.data(), size);

	cmd->BindPipeline(&pipeline);

	// Bind scene UBO (set 0)
	sceneUBO->Bind(cmd, pipeline, frameIndex);

	// Bind instance SSBO (set 1)
	instanceBuffer->Bind(cmd, pipeline, frameIndex);

	// Draw all boxes in one call
	cmd->Draw(24, static_cast<u32>(drawQueue.size()), 0, 0);

	drawQueue.clear();
}

void DebugRenderer::Cleanup()
{
	if (pipeline.vk != VK_NULL_HANDLE)
		pipeline.Destroy();
}
