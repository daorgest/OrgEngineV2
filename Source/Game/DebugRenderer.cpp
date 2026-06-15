#include "DebugRenderer.h"

#include "ShaderConstants.h"
#include "Tools/FileManager.h"

using namespace Renderer;

bool DebugRenderer::Initialize(GPUDevice* dev, GPUShaderBuffer* ubo, DescriptorAllocatorGrowable* alloc, bool depthTest, bool alwaysOnTop)
{
    this->device = dev;
    this->sceneUBO = ubo;
    drawQueue.reserve(maxInstances);

    shader = device->CreateShaderPath("Shaders/boundingBox.spv");

    DescriptorSetLayoutDesc instSetDesc = {
        .setIndex = 1,
        .bindings = {
                { .binding = 0, .type = DescriptorType::StorageBuffer, .stageFlags = ShaderStage::AllGraphics, .size = maxInstances * sizeof(Engine::BBoxPush) }
        }
    };

    instanceBuffer = device->CreateShaderBuffer(alloc, instSetDesc);

    // 2. Define Pipeline Layout using Constants
    PipelineLayoutDesc debugLayout = {
        .setLayouts = {
            DescriptorSetLayoutDesc::FromConstants(0, Constants::Scene),
            DescriptorSetLayoutDesc::FromConstants(1, instSetDesc.bindings)
        }
    };
    const auto depthOp = (alwaysOnTop || !depthTest) ? CompareOp::Always : CompareOp::Greater;
    const bool depthWrite = !alwaysOnTop && depthTest;


	GraphicsPipelineDesc debugDesc = {
		.vertexShader   = shader,
		.fragmentShader = shader,
		.raster = {
			.topology     = PrimitiveTopology::LineList,
			.cull         = CullMode::None,
			.depthFormat  = TextureFormat::D32_SFLOAT,
			.depthWrite   = depthWrite,
			.depthOp      = depthOp,
		    .sampleCount = device->currentSamples,
			.colorFormats = { TextureFormat::BGRA8_SRGB }
		},
		.layout = debugLayout
	};

    pipeline = device->CreateGraphicsPipeline(debugDesc);

    return pipeline.get();
}

void DebugRenderer::QueueBox(const glm::mat4& model, const AABB& aabb, const glm::vec4& boxColor)
{
    if (drawQueue.size() >= maxInstances) return;
    drawQueue.push_back({model, aabb.Min(), depthBias, aabb.Max(), flags, boxColor});
}

void DebugRenderer::Flush(GPUCommandBuffer* cmd, u32 frameIndex)
{
	if (drawQueue.empty())
		return;

	const size_t size = drawQueue.size() * sizeof(Engine::BBoxPush);

	// Upload all instance data
	instanceBuffer->UpdateBinding(frameIndex, 0, drawQueue.data(), size);

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
    pipeline.reset();
    instanceBuffer.reset();
    shader.reset();
    drawQueue.clear();
}
