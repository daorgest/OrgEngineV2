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

    DescriptorSetLayoutDesc instSetDesc;
    instSetDesc.setIndex = 1;
    instSetDesc.bindings = {
            {
                .binding = 0,
                .type = DescriptorType::StorageBuffer,
                .stageFlags = ShaderStage::AllGraphics,
                .size = maxInstances * sizeof(BBoxPush)
            }
    };
	instanceBuffer = std::make_unique<VulkanShaderBuffer>(device, globalDescriptorAllocator, instSetDesc);
	instanceBuffer->AllocateDescriptorSets();


    PipelineLayoutDesc debugLayout;
    debugLayout.setLayouts = {
            { .setIndex = 0, .bindings = { sceneUBO->desc.bindings } },
            { .setIndex = 1, .bindings = { instSetDesc.bindings } }
    };

    const auto depthOp = (alwaysOnTop || !depthTest) ? CompareOp::Always : CompareOp::Greater;
    const bool depthWrite = !alwaysOnTop && depthTest;


	GraphicsPipelineDesc debugDesc = {
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
		.layout = debugLayout
	};

    pipeline = device->CreateGraphicsPipeline(debugDesc);

    return pipeline != nullptr && pipeline->IsValid();
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

	cmd->BindPipeline(pipeline.get());

	// Bind scene UBO (set 0)
	sceneUBO->Bind(cmd, pipeline.get(), frameIndex);

	// Bind instance SSBO (set 1)
	instanceBuffer->Bind(cmd, pipeline.get(), frameIndex);

	// Draw all boxes in one call
	cmd->Draw(24, static_cast<u32>(drawQueue.size()), 0, 0);

	drawQueue.clear();
}

void DebugRenderer::Cleanup()
{
    ///
}
