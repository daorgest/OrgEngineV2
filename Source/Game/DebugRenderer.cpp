#include "DebugRenderer.h"
#include "Tools/Logger.h"
#include "VulkanShader.h"
#include "Tools/Arena.h"

using namespace Renderer;

bool DebugRenderer::Initialize(VulkanDevice* dev, ArenaAllocator* arena, VulkanShaderBuffer* sceneUBO,
				DescriptorAllocatorGrowable* globalDescriptorAllocator, bool depthTest, bool alwaysOnTop)
{
	device = dev;
	this->sceneUBO = sceneUBO;

	auto codeResult = VulkanShader::ReadShaderFile("Shaders/boundingBox.spv");
	if (!codeResult)
	{
		LOG(Error, "Failed to read shader 'Shaders/boundingBox.spv' ({})", static_cast<int>(codeResult.error()));
		return false;
	}
	shader = arena->Emplace<VulkanShader>(device, codeResult.value());

	// Shader Instance buffer!!
	UniformBufferDesc instDesc = {
		.setIndex = 1,
		.stageFlags = ShaderStage::Vertex,
		.bindings = {
			{0, DescriptorType::StorageBuffer, maxInstances * sizeof(BBoxPush)},
		}
	};
	instanceBuffer = arena->Emplace<VulkanShaderBuffer>(device,globalDescriptorAllocator,instDesc);
	instanceBuffer->AllocateDescriptorSets();


	Array setLayouts = {
		sceneUBO->layout.vk,       // set = 0
		instanceBuffer->layout.vk  // set = 1
	};

	PipelineLayoutDesc layoutDesc{};
	layoutDesc.setLayouts = setLayouts;

	VulkanPipelineBuilder pb;
	pb.SetFragVerShaders(shader->shader, shader->shader)
	  .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
	  .SetPolygonMode(VK_POLYGON_MODE_LINE)
	  .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
	  .SetMultisamplingNone()
	  .DisableBlending()
	  .SetColorAttachmentFormat(TextureFormat::BGRA8_SRGB)
	  .SetDepthAttachmentFormat(TextureFormat::D32_SFLOAT);

	if (alwaysOnTop)
		pb.EnableDepthTest(false, VK_COMPARE_OP_ALWAYS);  // No depth test, always visible
	else if (depthTest)
		pb.EnableDepthTest(true, VK_COMPARE_OP_LESS);     // Standard depth test with writes
	else
		pb.EnableDepthTest(false, VK_COMPARE_OP_LESS);    // Depth test without writes

	pb.Layout(pipeline.vkLayout);

	if (!pipeline.Create(device, layoutDesc, pb))
	{
		LOG(Error, "Failed to create AABB debug pipeline");
		return false;
	}

	LOG(Info, "AABB Debug Pipeline created");
	return true;
}

void DebugRenderer::QueueBox(const glm::mat4& model, const glm::vec3& min, const glm::vec3& max)
{
	drawQueue.push_back({model, min, depthBias, max, flags, color});
}

void DebugRenderer::Flush(VkCommandBuffer cmd, u32 frameIndex)
{
	if (drawQueue.empty())
		return;

	const u32 count = drawQueue.size();
	const size_t size = count * sizeof(BBoxPush);

	// Upload all instance data
	instanceBuffer->Update(frameIndex, drawQueue.data(), size);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.vk);

	// Bind scene UBO (set 0)
	sceneUBO->Bind(cmd, pipeline, frameIndex);

	// Bind instance SSBO (set 1)
	instanceBuffer->Bind(cmd, pipeline, frameIndex);

	// Draw all boxes in one call
	vkCmdDraw(cmd, 24, count, 0, 0);

	drawQueue.clear();
}

void DebugRenderer::Cleanup()
{
	if (pipeline.vk != VK_NULL_HANDLE)
		pipeline.Destroy();
	if (shader) { shader->Destroy(); shader = nullptr; }
}
