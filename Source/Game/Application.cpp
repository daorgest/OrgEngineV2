//
// Created by Orgest on 7/13/2025.
//

#include "Application.h"

#include <algorithm>
#include <chrono>

#include "imgui.h"
#include "MeshData.h"
#include "MeshLoader.h"
#include "MeshStats.h"
#include "TextureManager.h"
#include "VulkanPipeline.h"
#include "../Core/Tools/Timer.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"
#include "Input/InputSys.h"
#include "Input/InputSysGameInput.h"
#include "tracy/Tracy.hpp"

void Application::DrawLoadingSplash(const char* text) const
{
	// Process a frame just like Run() does, but only once.
	if (renderer->ResizeIfNeeded()) return;
	if (swapchain->needsRecreation)
		swapchain->Recreate();

	auto frame = renderer->BeginFrame();
	if (frame.commandContext == nullptr)
	{
		return;
	}

	// Build a minimal ImGui overlay
	EditorUI::BeginFrame();
	{
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(vp->WorkSize);
		constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBackground;
		if (ImGui::Begin("##LoadingSplash", nullptr, flags))
		{
			// Center the text
			ImVec2 win = ImGui::GetWindowSize();
			ImVec2 sz = ImGui::CalcTextSize(text);
			ImGui::SetCursorPos({(win.x - sz.x) * 0.5f, (win.y - sz.y) * 0.5f});
			ImGui::TextUnformatted(text);
		}
		ImGui::End();
	}

	// Record a single pass: clear to black, draw ImGui, present
	VkCommandBuffer cmd = frame.commandContext->commandBuffer;

	auto& colorImage = swapchain->images[frame.imageIndex];
	colorImage.Transition(cmd, TextureLayout::ColorWrite);

	Renderer::RenderPassDesc rpDesc = {
		.renderPasses = SPAN_ONE(colorImage),
		.depthTexture = nullptr,
	};

	renderPass->Begin(cmd, swapchain->GetExtent(), rpDesc, true);

	editorUI->EndFrame();
	editorUI->Render(cmd);

	renderPass->End(cmd);
	colorImage.Transition(cmd, TextureLayout::Present);

	renderer->EndFrame(frame);
}

bool Application::CreateAabbPipeline(bool depthTest /*=true*/, bool alwaysOnTop /*=false*/)
{
	// Load shader
	Vector<u32> code = Renderer::VulkanShader::ReadShaderFile("Shaders/boundingBox.spv");
	if (code.empty())
	{
		LOG(Error, "AABB: failed to read Shaders/aabb.spv");
		return false;
	}
	aabbShader = renderArena.Emplace<Renderer::VulkanShader>(device, std::span<const u32>(code));

	// Pipeline layout: set=0 (scene UBO) + push constants
	Array<VkDescriptorSetLayout, 1> setLayouts = {
		sceneUBO->layout
	};

	VkPushConstantRange pcRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(BBoxPush)
	};

	Renderer::PipelineLayoutDesc pipeDesc = {
		.setLayouts = setLayouts,
		.pushRanges = SPAN_ONE(pcRange)
	};

	// Build graphics pipeline
	Renderer::VulkanPipelineBuilder pb;
	pb.SetFragVerShaders(aabbShader->shader, aabbShader->shader)
	  .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
	  .SetPolygonMode(VK_POLYGON_MODE_FILL)
	  .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
	  .SetMultisamplingNone()
	  .DisableBlending()
	  .SetColorAttachmentFormat(swapchain->surfaceFormat.format)
	  .SetDepthAttachmentFormat(VK_FORMAT_D32_SFLOAT)
	  .Layout(aabbPipeline.vkLayout);

	if (alwaysOnTop) pb.EnableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
	else if (depthTest) pb.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	else pb.EnableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);


	if (aabbPipeline.Create(device, pipeDesc, pb) == false)
	{
		return false;
	}


	return (aabbPipeline != VK_NULL_HANDLE);
}

Renderer::VulkanImage CreateCubeMap(Renderer::VulkanDevice* device, Array<const char*, 6>& paths)
{
	Array<TextureData, 6> img;

	for (int i = 0; i < 6; i++)
	{
		auto td = TextureManager::LoadTextureFromSTB(paths[i], true);
		if (td.has_value())
		{
			img[i] = std::move(*td);
		}
		else
		{
			LOG(Warning, "Missing face {}", i);
		}
	}


	// checking to see if images are same size
	for (int i = 1; i < img.size(); i++)
	{
		if (img[i].width != img[0].width || img[i].height != img[0].height)
		{
			LOG(Error, "CreateCubeMap: face mismatch (0={}x{}, {}={}x{})",
			    img[0].width, img[0].height, i, img[i].width, img[i].height);
			return Renderer::VulkanImage{};
		}
	}

	const u32 w = img[0].width;
	const u32 h = img[0].height;

	TextureInfo info{
		.extent = {w, h, 1},
		.mipLevels = 1,
		.arrayLayers = 6,
		.type = ImageType::CubeMap,
		.format = TextureFormat::RGBA8_SRGB,
		.dimension = TextureDimension::CubeMap,
		.usage = ImageUsage::Sampled | ImageUsage::TransferDst,
	};

	Renderer::VulkanImage cube(device, info);

	const size_t faceBytes = static_cast<size_t>(w) * h * 4;
	Vector<u8> packed(faceBytes * 6);
	for (u32 i = 0; i < 6; ++i) {
		std::memcpy(packed.data() + (i * faceBytes), img[i].data.data(), faceBytes);
}

	cube.UploadTextureToGPU(packed.data(), info);

	return cube;
}

inline void Application::QueueBBoxWS(const glm::mat4& model, const glm::vec3& mn, const glm::vec3& mx, const glm::vec4& color,
                                     float depthBias, uint32_t flags)
{
	bboxQueue.push_back(BBoxPush{model, mn, depthBias, mx, flags, color});
}

inline void Application::FlushBBoxWS(VkCommandBuffer cmd, uint32_t frameIndex)
{
	if (bboxQueue.empty() || aabbPipeline == VK_NULL_HANDLE) return;

	// bind once
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, aabbPipeline);
	sceneUBO->Bind(cmd, aabbPipeline, frameIndex, 0); // set=0 only

	// push constants + draw (24 verts) per box
	for (const auto& [model, aabbMin, depthBias, aabbMax, flags, color] : bboxQueue)
	{
		BBoxPush pc{};
		pc.model = model;
		pc.aabbMin = aabbMin;
		pc.depthBias = depthBias;
		pc.aabbMax = aabbMax;
		pc.flags = flags;
		pc.color = color;

		vkCmdPushConstants(cmd, aabbPipeline.vkLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BBoxPush), &pc);

		vkCmdDraw(cmd, 24, 1, 0, 0);
	}

	bboxQueue.clear();
}

bool Application::CreateSkybox()
{
	ZoneScopedN("CreateSkyboxInline");

	const Array<Vertex, 24> kSkyVerts = {
		// Front (+Z)
		{{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {1, 0, 0}, {0, 0}},
		{{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {0, 1, 0}, {1, 0}},
		{{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {0, 0, 1}, {1, 1}},
		{{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {1, 1, 1}, {0, 1}},

		// Back (-Z)
		{{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {1, 1, 0}, {0, 0}},
		{{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {0, 1, 1}, {1, 0}},
		{{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {1, 0, 1}, {1, 1}},
		{{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {0.5f, 0.5f, 0.5f}, {0, 1}},

		// Right (+X)
		{{0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {1, 0, 0}, {0, 0}},
		{{0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {0, 1, 0}, {1, 0}},
		{{0.5f, 0.5f, -0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {0, 0, 1}, {1, 1}},
		{{0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {1, 1, 1}, {0, 1}},

		// Left (-X)
		{{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {1, 1, 0}, {0, 0}},
		{{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {0, 1, 1}, {1, 0}},
		{{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {1, 0, 1}, {1, 1}},
		{{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {0.5f, 0.5f, 0.5f}, {0, 1}},

		// Top (+Y)
		{{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {1, 0, 0}, {0, 0}},
		{{0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {0, 1, 0}, {1, 0}},
		{{0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {0, 0, 1}, {1, 1}},
		{{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {1, 1, 1}, {0, 1}},

		// Bottom (-Y)
		{{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {1, 1, 0}, {0, 0}},
		{{0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {0, 1, 1}, {1, 0}},
		{{0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {1, 0, 1}, {1, 1}},
		{{-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {0.5f, 0.5f, 0.5f}, {0, 1}},
	};

	const Array<u32, 36> kSkyIdx = {
		0, 1, 2, 2, 3, 0, // Front
		4, 5, 6, 6, 7, 4, // Back
		8, 9, 10, 10, 11, 8, // Right
		12, 13, 14, 14, 15, 12, // Left
		16, 17, 18, 18, 19, 16, // Top
		20, 21, 22, 22, 23, 20 // Bottom
	};

	MeshPart skyPart = {
		Vector<Vertex>(kSkyVerts.begin(), kSkyVerts.end()),
		Vector<u32>(kSkyIdx.begin(), kSkyIdx.end())
	};


	Array skyFaces = {
		"skybox/right.jpg", // +X
		"skybox/left.jpg", // -X
		"skybox/top.jpg", // +Y
		"skybox/bottom.jpg", // -Y
		"skybox/front.jpg", // +Z
		"skybox/back.jpg" // -Z
	};


	// 2) Cubemap + sampler
	skyCubeMap = CreateCubeMap(device, skyFaces);
	if (skyCubeMap.image == nullptr)
	{
		LOG(Error, "Skybox: failed to create cubemap image.");
		return false;
	}

	SamplerDesc sampDesc{
		.minFilter = SamplerFilter::Linear,
		.magFilter = SamplerFilter::Linear,
		.mipFilter = SamplerMipFilter::None,
		.addressU = SamplerAddressMode::ClampToEdge,
		.addressV = SamplerAddressMode::ClampToEdge,
		.addressW = SamplerAddressMode::ClampToEdge,
	};
	skySampler = Renderer::VulkanSampler(device, sampDesc);

	// 3) Model + material + descriptors
	skyModel = std::make_unique<Renderer::VulkanModel>();
	skyModel->parts.emplace_back();
	skyModel->parts.back().Create(device, skyPart);

	Renderer::DescriptorLayout skyLayout;
	{
		Renderer::DescriptorLayoutBuilder b;
		b.AddBinding(0, DescriptorType::SampledImage);
		b.AddBinding(1, DescriptorType::Sampler);
		skyLayout = b.Build(device->device, ShaderStage::Fragment | ShaderStage::Vertex);
	}
	skyModel->layout = skyLayout;

	Array<Renderer::DescriptorAllocatorGrowable::PoolSizeRatio, 2> poolSizes = {
		{DescriptorType::SampledImage, 2.f},
		{DescriptorType::Sampler, 1.f}
	};
	skyModel->descriptorPool.Init(device, 1, poolSizes);

	skyModel->materials.clear();
	skyModel->materials.emplace_back();
	Renderer::VulkanMaterial& mat = skyModel->materials.back();
	mat.colorImage = &skyCubeMap;
	mat.sampler = &skySampler;
	mat.descriptorSet = skyModel->descriptorPool.Allocate(skyModel->layout);

	Renderer::DescriptorWriter()
		.WriteImage(0, mat.colorImage, nullptr, DescriptorType::SampledImage)
		.WriteImage(1, std::nullopt, mat.sampler, DescriptorType::Sampler)
		.UpdateSet(device->device, mat.descriptorSet.vk);

	Vector<u32> skyCode = Renderer::VulkanShader::ReadShaderFile("Shaders/skybox.spv");
	if (skyCode.empty())
	{
		LOG(Error, "Skybox: failed to read Shaders/skybox.spv");
		return false;
	}
	skyShader = renderArena.Emplace<Renderer::VulkanShader>(device, skyCode);


	Array<VkDescriptorSetLayout, 2> setLayouts = {
		sceneUBO->layout,
		skyModel->layout
	};

	VkPushConstantRange pcRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(PushConstants)
	};

	Renderer::PipelineLayoutDesc layoutDesc = {
		.setLayouts = setLayouts,
		.pushRanges = SPAN_ONE(pcRange)
	};


	Renderer::VulkanPipelineBuilder pb;
	pb.SetFragVerShaders(skyShader->shader, skyShader->shader)
	  .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	  .SetPolygonMode(VK_POLYGON_MODE_FILL)
	  .SetCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE) // inside of cube
	  .SetMultisamplingNone()
	  .DisableBlending()
	  .EnableDepthTest(false, VK_COMPARE_OP_GREATER_OR_EQUAL)
	  .SetColorAttachmentFormat(swapchain->surfaceFormat.format)
	  .SetDepthAttachmentFormat(VK_FORMAT_D32_SFLOAT)
	  .Layout(skyPipeline.vkLayout);

	return skyPipeline.Create(device, layoutDesc, pb) != false;
}

bool Application::Init()
{
	ZoneScopedN("Application::Init");
	Log::InitLogFile();

	// Platform & Vulkan setup
	{
		ZoneScopedN("Init Platform & Window");
		wc = coreArena.Emplace<Platform::WindowContext>();
		instance = coreArena.Emplace<Renderer::VulkanInstance>();
		device = coreArena.Emplace<Renderer::VulkanDevice>();
		swapchain = coreArena.Emplace<Renderer::VulkanSwapchain>();
		renderer = coreArena.Emplace<Renderer::VulkanRenderer>();
		renderPass = coreArena.Emplace<Renderer::VulkanRenderPass>();
		globalDescriptorAlloc = coreArena.Emplace<Renderer::DescriptorAllocatorGrowable>();
		editorUI = coreArena.Emplace<EditorUI>();
		Platform::Init(wc);
		Platform::ShowWindow(*wc);
	}

	{
		ZoneScopedN("Init Vulkan & Swapchain");
		instance->Init();
		device->Init(instance);
		swapchain->Init(device, wc->handle);
	}
	{
		ZoneScopedN("Init Global Descriptor Allocator");
		Array<Renderer::DescriptorAllocatorGrowable::PoolSizeRatio, 5> sizes = {
			{DescriptorType::StorageImage, 2.f},
			{DescriptorType::UniformBuffer, 2.f},
			{DescriptorType::SampledImage, 3.f},
			{DescriptorType::Sampler, 4.f},
			{DescriptorType::StorageBuffer, 2.f},
		};
		globalDescriptorAlloc->Init(device, 4, sizes);
	}
	{
		ZoneScopedN("Init Renderer & UI");
		renderer->Init(device, swapchain);
		editorUI->Init(instance, device, swapchain);
	}

	DrawLoadingSplash("Loading...");

	{
		ZoneScopedN("Init Scene UBO");
		UniformBufferDesc sceneDesc = {
			.stageFlags = ShaderStage::AllGraphics,
			.bindings = {
				{2, DescriptorType::UniformBuffer, sizeof(DebugUBO)},
				{3, DescriptorType::UniformBuffer, sizeof(CameraUBO)},
				{4, DescriptorType::StorageBuffer, sizeof(LightUBO) * 8},
				{5, DescriptorType::UniformBuffer, sizeof(LightMeta)},
				{6, DescriptorType::UniformBuffer, sizeof(SceneUBO)},
				{7, DescriptorType::UniformBuffer, sizeof(NormalMatrixUBO)},
			}
		};
		sceneUBO = coreArena.Emplace<Renderer::VulkanShaderBuffer>(device, globalDescriptorAlloc, sceneDesc);
		sceneUBO->AllocateDescriptorSets();
	}

	// Checkerboard/normal fallback
	SamplerDesc samplerDesc{};
	samplerDesc.minFilter = SamplerFilter::Nearest;
	samplerDesc.magFilter = SamplerFilter::Nearest;
	samplerDesc.mipFilter = SamplerMipFilter::None;
	checkerboardImage = Renderer::VulkanImage::CreateCheckerboardTexture(*device);
	checkerboardSampler = coreArena.Emplace<Renderer::VulkanSampler>(device, samplerDesc);
	normalFallbackImage = Renderer::VulkanImage::CreateDefaultNormalMap(*device);
	Renderer::TextureOrFallback fallback = {
		.fallbackImage = &checkerboardImage,
		.fallbackSampler = checkerboardSampler,
		.fallbackNormalImage = &normalFallbackImage,
	};

	// Main model
	{
		ZoneScopedN("LoadModel From Source!");
		DrawLoadingSplash("Loading OBJ Model...");
		auto result = Assets::MeshLoader::LoadModelFromSource(MeshSourceType::OBJ, "Sponza/sponza.obj");
		modelInst = std::make_unique<Renderer::VulkanModel>(device, *result, fallback);
		Renderer::ModelComponent comp = {

			.model = modelInst.get(),
			.transform = glm::scale(glm::vec3(0.01f))
		};
		models.push_back(comp);
	}

	// Cube mesh
	const Array<Vertex, 24> cubeVertices = {
		// Front (+Z)
		{{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {1, 0, 0}, {0, 0}},
		{{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {0, 1, 0}, {1, 0}},
		{{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {0, 0, 1}, {1, 1}},
		{{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0, 0, +1}, {1, 1, 1}, {0, 1}},
		// Back (-Z)
		{{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {1, 1, 0}, {0, 0}},
		{{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {0, 1, 1}, {1, 0}},
		{{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {1, 0, 1}, {1, 1}},
		{{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {-1, 0, 0, +1}, {0.5f, 0.5f, 0.5f}, {0, 1}},
		// Right (+X)
		{{0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {1, 0, 0}, {0, 0}},
		{{0.5f, -0.5f, -0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {0, 1, 0}, {1, 0}},
		{{0.5f, 0.5f, -0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {0, 0, 1}, {1, 1}},
		{{0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 0, -1, +1}, {1, 1, 1}, {0, 1}},

		// Left (-X)
		{{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {1, 1, 0}, {0, 0}},
		{{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {0, 1, 1}, {1, 0}},
		{{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {1, 0, 1}, {1, 1}},
		{{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {0, 0, 1, +1}, {0.5f, 0.5f, 0.5f}, {0, 1}},

		// Top (+Y)
		{{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {1, 0, 0}, {0, 0}},
		{{0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {0, 1, 0}, {1, 0}},
		{{0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {0, 0, 1}, {1, 1}},
		{{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 0, 0, +1}, {1, 1, 1}, {0, 1}},

		// Bottom (-Y)
		{{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {1, 1, 0}, {0, 0}},
		{{0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {0, 1, 1}, {1, 0}},
		{{0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {1, 0, 1}, {1, 1}},
		{{-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0, 0, +1}, {0.5f, 0.5f, 0.5f}, {0, 1}},
	};

	const Array<u32, 36> cubeIndices = {
		0, 1, 2, 2, 3, 0, // Front
		4, 5, 6, 6, 7, 4, // Back
		8, 9, 10, 10, 11, 8, // Right
		12, 13, 14, 14, 15, 12, // Left
		16, 17, 18, 18, 19, 16, // Top
		20, 21, 22, 22, 23, 20 // Bottom
	};

	// its fine (it's not) that I kinda want it to be views instead
	MeshPart cubePart{
		Vector<Vertex>(cubeVertices.begin(), cubeVertices.end()),
		Vector<u32>(cubeIndices.begin(), cubeIndices.end()),
	};

	cubeMesh = std::make_unique<Renderer::VulkanModel>();
	cubeMesh->parts.emplace_back();
	cubeMesh->parts.back().Create(device, cubePart);

	cubeMesh->layout = models.back().model->layout;

	Array<Renderer::DescriptorAllocatorGrowable::PoolSizeRatio, 2> sizes = {
		{DescriptorType::SampledImage, 2.f},
		{DescriptorType::Sampler, 1.f}
	};
	cubeMesh->descriptorPool.Init(device, 1, sizes);

	cubeMesh->materials.clear();
	cubeMesh->materials.emplace_back();
	Renderer::VulkanMaterial& cubeMat = cubeMesh->materials.back();
	cubeMat.colorImage = &checkerboardImage; // sample cube will have checkerboard
	cubeMat.sampler = checkerboardSampler;
	cubeMat.normalImage = fallback.fallbackNormalImage;
	cubeMat.descriptorSet = cubeMesh->descriptorPool.Allocate(cubeMesh->layout);

	Renderer::DescriptorWriter()
		.WriteImage(0, cubeMat.colorImage, nullptr, DescriptorType::SampledImage)
		.WriteImage(1, std::nullopt, cubeMat.sampler, DescriptorType::Sampler)
		.WriteImage(2, cubeMat.normalImage, nullptr, DescriptorType::SampledImage)
		.UpdateSet(device->device, cubeMat.descriptorSet.vk);

	Renderer::ModelComponent cubeComp{};
	cubeComp.model = cubeMesh.get();
	cubeComp.transform = glm::translate(glm::vec3(0.0f, 2.0f, 0.0f));
	models.push_back(cubeComp);

	DrawLoadingSplash("Building Skybox...");
	CreateSkybox();
	DrawLoadingSplash("Compiling Shaders...");
	Vector<u32> code = Renderer::VulkanShader::ReadShaderFile("Shaders/scene.spv");
	shader = renderArena.Emplace<Renderer::VulkanShader>(device, code);
	// auto* shader = coreArena.Emplace<Renderer::VulkanShader>(device, SCENE_SPV, SCENE_SPV_SIZE, ShaderFormat::SPIRV);

	// I have to abstract this.
	Array setLayouts = {
		sceneUBO->layout.vk,
		models[0].model->layout // this should be the loaded model (obj, fbx, etc)....ew...this is bad	
	};

	VkPushConstantRange scenePushConstants = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(PushConstants)
	};

	Renderer::PipelineLayoutDesc layoutDesc = {
		.setLayouts = setLayouts,
		.pushRanges = SPAN_ONE(scenePushConstants)
	};

	Renderer::VulkanPipelineBuilder builder;
	builder
		.SetFragVerShaders(shader->shader, shader->shader)
		.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetPolygonMode(VK_POLYGON_MODE_FILL)
		.SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.EnableMultisampling(VK_SAMPLE_COUNT_2_BIT)
		.SetMultisamplingNone()
		.DisableBlending()
		.EnableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
		.SetColorAttachmentFormat(swapchain->surfaceFormat.format)
		.SetDepthAttachmentFormat(VK_FORMAT_D32_SFLOAT)
		.Layout(scenePipeline.vkLayout);

	scenePipeline.Create(device, layoutDesc, builder);


	CreateAabbPipeline(true, false);

	ComputeStaticSceneStats();

	// Initial Camera position
	camera.position = {1.77f, 1.21f, -1.70f};
	camera.forward = glm::normalize(glm::vec3{-0.71f, -0.17f, 0.69f});
	camera.up = {0, 1, 0};
	camera.UpdateDirectionVectors();

	// // Initialize FPS camera from base camera
	// fpsCamera.position = camera.position;
	// fpsCamera.yaw = camera.yaw;
	// fpsCamera.pitch = camera.pitch;
	// fpsCamera.fov = camera.fov;
	// fpsCamera.UpdateDirectionVectors();
	// fpsCamera.SyncBodyFromCameraStanding();

	activeCamera = &camera;
	camMode = CameraMode::FreeFly;

	lights.clear();
	{
		LightUBO L{};
		L.type = static_cast<u32>(LightType::Directional);
		L.direction = {-0.8f, -0.5f, -0.053f};
		L.color = {1.0f, 0.95f, 0.85f};
		L.intensity = 13.0f;
		lights.push_back(L);
	}
	lightMeta.count = lights.size();
	debugData.debugMode = DebugView::Material;

	return true;
}

void Application::UpdateCamera()
{
	const float dt = wc->GetDeltaTime();

	const bool focused = wc->displayState.isFocused;
	ImGuiIO& ioImgui = ImGui::GetIO();

	// Sticky state for focus/locking + one-shot delta suppression
	static bool sLocked = false;
	static bool sHadFocus = true;

	// --- toggles FIRST ---
	if (input.keyboard[Keyboard::F2].pressed)
		drawAABBs = !drawAABBs;

	if (input.keyboard[Keyboard::F1].pressed)
	{
		if (camMode == CameraMode::FreeFly)
		{
			// FreeFly -> FPS (carry pose)
			fpsCamera.position = camera.position;
			fpsCamera.yaw = camera.yaw;
			fpsCamera.pitch = camera.pitch;
			fpsCamera.fov = camera.fov;
			fpsCamera.UpdateDirectionVectors();
			fpsCamera.SyncBodyFromCameraStanding();

			camMode = CameraMode::FPS;
			activeCamera = &fpsCamera;

			if (focused && !sLocked) {
				Platform::LockCursor(*wc, true);
				sLocked = true;
			}
		}
		else
		{
			// FPS -> FreeFly (carry pose back)
			camera.position = fpsCamera.position;
			camera.yaw = fpsCamera.yaw;
			camera.pitch = fpsCamera.pitch;
			camera.fov = fpsCamera.fov;
			camera.UpdateDirectionVectors();

			camMode = CameraMode::FreeFly;
			activeCamera = &camera;


			if (sLocked) {
				Platform::LockCursor(*wc, false);
				sLocked = false;
			}
		}
	}

	// Allow mouse look when focused and ImGui isn't capturing
	input.mouseLookActive = focused && !ioImgui.WantCaptureMouse;

	// --- focus change handling ---
	if (focused != sHadFocus)
	{
		if (!focused)
		{
			if (sLocked)
			{
				Platform::LockCursor(*wc, false);
				Input::ResetInputOnFocusLoss(); // your helper should release all held keys/buttons safely
				sLocked = false;
			}
		}
		else
		{
			if (camMode == CameraMode::FPS && !sLocked) {
				Platform::LockCursor(*wc, true);
				sLocked = true;
			}
		}

		sHadFocus = focused;
	}

	// Lock/unlock based on current mode
	const bool wantLock = focused && (camMode == CameraMode::FPS);
	if (sLocked != wantLock)
	{
		Platform::LockCursor(*wc, wantLock);
		sLocked = wantLock;
	}

	// If window not focused, do minimal bookkeeping and bail
	if (!focused)
	{
		Input::EndFrameInputUpdate();
		return;
	}

	// --- FPS MODE ---
	if (camMode == CameraMode::FPS)
	{
		// Let FPS camera own look + movement (uses input inside)
		fpsCamera.Update(dt);

		// Scroll adjusts speed popup (only when mouselook is active)
		if (input.scrollY != 0 && input.mouseLookActive)
			cameraSpeedPopupTime = 1.5f;
		input.scrollY = 0;
		if (cameraSpeedPopupTime > 0.0f) cameraSpeedPopupTime -= dt;
		Input::EndFrameInputUpdate();
		return;
	}

	// --- FREEFLY MODE ---

	// Mouse-look (hold LMB)
	if (input.mouseLookActive && input.mouseButtons[Mouse::Left].held)
	{
		constexpr float MOUSE_SENS = 0.1f;
		camera.yaw -= static_cast<f32>(input.xrel * MOUSE_SENS);
		camera.pitch -= static_cast<f32>(input.yrel * MOUSE_SENS);
		camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);
		camera.UpdateDirectionVectors();
		input.xrel = input.yrel = 0.0f; // consume deltas
	}

	// WASDQE movement in camera space
	glm::vec3 move{0, 0, 0};
	if (input.keyboard[Keyboard::W].held) move += camera.forward;
	if (input.keyboard[Keyboard::S].held) move -= camera.forward;
	if (input.keyboard[Keyboard::A].held) move -= camera.right;
	if (input.keyboard[Keyboard::D].held) move += camera.right;
	if (input.keyboard[Keyboard::Q].held) move -= camera.up;
	if (input.keyboard[Keyboard::E].held) move += camera.up;
	if (glm::length2(move) > 1.0f) move = glm::normalize(move);

	// Alt + MMB pan
	if (input.mouseButtons[Mouse::Middle].held && input.keyboard[Keyboard::Alt].held)
	{
		constexpr float PAN_SENS = 0.04f;
		camera.position += camera.right * (static_cast<f32>(-input.xrel) * PAN_SENS);
		camera.position += camera.up * (static_cast<f32>(input.yrel) * PAN_SENS);
		input.xrel = input.yrel = 0.0f; // consume deltas
	}

	camera.position += move * cameraSpeed * dt;

	// Camera speed handling via scroll (only when mouselook is active)
	if (input.scrollY != 0 && input.mouseLookActive)
	{
		constexpr float kStep = 0.02f;
		cameraSpeed += input.scrollY * kStep;
		cameraSpeed = std::clamp(cameraSpeed, 0.1f, 100.0f);
		cameraSpeedPopupTime = 1.5f;
	}
	input.scrollY = 0;
	if (cameraSpeedPopupTime > 0.0f) cameraSpeedPopupTime -= dt;

	Input::EndFrameInputUpdate();
}

void Application::UpdateSceneUBO(const Renderer::FrameContext& frame)
{
	ZoneScopedN("UpdateSceneUBO");

	aspectRatio = static_cast<float>(swapchain->width) / static_cast<float>(swapchain->height);
	sceneData.view = activeCamera->GetViewMatrix();
	sceneData.proj = activeCamera->GetProjectionMatrix(aspectRatio);

	camUBO.position = activeCamera->position;
	camUBO.nearPlane = activeCamera->nearPlane;
	camUBO.farPlane = activeCamera->farPlane;

	// keep flashlight glued to camera
	for (auto& l : lights)
	{
		if (l.type == LightType::Spot)
		{
			l.position = activeCamera->position;
			l.direction = glm::normalize(activeCamera->forward);
			break;
		}
	}

	lightMeta.count = lights.size();

	const u32 frameIndex = frame.frameIndex;
	sceneUBO->UpdateBinding(frameIndex, 2, &debugData, sizeof(DebugUBO));
	sceneUBO->UpdateBinding(frameIndex, 3, &camUBO, sizeof(CameraUBO));
	sceneUBO->UpdateBinding(frameIndex, 4, lights.data(), sizeof(LightUBO) * lights.size());
	sceneUBO->UpdateBinding(frameIndex, 5, &lightMeta, sizeof(LightMeta));
	sceneUBO->UpdateBinding(frameIndex, 6, &sceneData, sizeof(SceneUBO));
}

void Application::RenderScene(const Renderer::FrameContext& frame)
{
	VkCommandBuffer cmd = frame.commandContext->commandBuffer;
	TracyVkZone(frame.commandContext->tracyCtx, cmd, "RenderScene");
	const u32 imageIndex = frame.imageIndex;
	const Extent2D extent = swapchain->GetExtent();

	auto& colorImage = swapchain->images[imageIndex];
	auto& depthImage = swapchain->depthImage;
	const auto& queryPool = frame.frameData->queryPool;

	// Fetch GPU timing from PREVIOUS frame (that just finished via fence wait in BeginFrame)
	if (frame.frameData->queryPool.FetchResults())
	{
		sceneStats.gpuDrawTime = frame.frameData->queryPool.DeltaMs(0, 3);
	}

	Renderer::RenderPassDesc renderPassDesc
	{
		.renderPasses = SPAN_ONE(colorImage), // just the 1 image
		.depthTexture = &depthImage,
	};

	// Transition attachments
	{
		TracyVkZone(frame.commandContext->tracyCtx, cmd, "Scene/Transitions");
		colorImage.Transition(cmd, TextureLayout::ColorWrite);
		depthImage.Transition(cmd, TextureLayout::DepthWrite);
	}

	// Begin render pass

	renderPass->Begin(cmd, extent, renderPassDesc, true);

	// Set viewport, bind pipeline and UBO
	Renderer::VulkanRenderer::SetViewportAndScissor(cmd, extent);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline);
	sceneUBO->Bind(cmd, scenePipeline, frame.frameIndex, 0);

	// Start scene timing
	sceneStats.drawCallCount = 0;
	const auto cpuStart = std::chrono::high_resolution_clock::now();
	queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0);

	{
    Renderer::DrawCache dc{ .layout = scenePipeline.vkLayout };
    dc.lastMat      = nullptr;
    dc.lastMatSet   = VK_NULL_HANDLE;
    dc.lastIndex    = VK_NULL_HANDLE;
    dc.lastIndexOffset = ~VkDeviceSize{0};

    // pipeline + set=0 (scene UBO) should already be bound before this block

    for (const auto& inst : models)
    {
        const Renderer::VulkanModel* mdl = inst.model;
        if (!mdl) continue;

        const glm::mat4 instM = inst.transform;

        for (const auto& part : mdl->parts)
        {
            if (part.indexCount == 0) continue;
            if (part.materialIndex >= mdl->materials.size()) continue;

            const Renderer::VulkanMaterial& mat = mdl->materials[part.materialIndex];
            const VkDescriptorSet matSet = mat.descriptorSet.vk;

            // Bind set=1 only when the VkDescriptorSet handle changes
            if (matSet != dc.lastMatSet)
            {
                dc.lastMatSet = matSet;
                dc.lastMat    = &mat; // optional: keep pointer if you use it elsewhere

                vkCmdBindDescriptorSets(
                    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dc.layout,
                    /*firstSet*/1, /*descriptorSetCount*/1, &matSet,
                    /*dynCount*/0, /*dynOffsets*/nullptr
                );
            }

            // Bind index buffer only when changed
            constexpr VkDeviceSize kIdxOffset = 0;
            const VkBuffer ibuf = part.indexBuffer.buffer;
            if (ibuf != dc.lastIndex || dc.lastIndexOffset != kIdxOffset)
            {
                vkCmdBindIndexBuffer(cmd, ibuf, kIdxOffset, VK_INDEX_TYPE_UINT32);
                dc.lastIndex       = ibuf;
                dc.lastIndexOffset = kIdxOffset;
            }

            // Push constants per-part
            PushConstants pc{};
            pc.worldMatrix   = (part.transform == glm::mat4(1.0f)) ? instM : (instM * part.transform);
            pc.deviceAddress = part.vertexAddress;

            vkCmdPushConstants(
                cmd, dc.layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(PushConstants), &pc
            );

            // Draw
            vkCmdDrawIndexed(cmd, part.indexCount, 1, 0, 0, 0);

            if (drawAABBs)
            {
                QueueBBoxWS(pc.worldMatrix, part.localBounds.Min(), part.localBounds.Max(),
                            aabbColor, aabbBias, aabbFlags);
            }

            ++sceneStats.drawCallCount;
        }
    }
}
	queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 1);

	if (drawAABBs)
	{
		TracyVkZone(frame.commandContext->tracyCtx, cmd, "DrawAABBs");
		FlushBBoxWS(cmd, frame.frameIndex);
	}

	if (skyModel)
	{
		TracyVkZone(frame.commandContext->tracyCtx, cmd, "Skybox");
		queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 2);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline);

		// Bind set=0 (scene UBO) with skybox pipeline layout
		sceneUBO->Bind(cmd, skyPipeline, frame.frameIndex, 0);

		const auto& part = skyModel->parts[0];
		const auto& mat = skyModel->materials[part.materialIndex];

		// Bind set=1 (skybox material: cube image + sampler)
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline.vkLayout,
		                        1, 1, &mat.descriptorSet.vk, 0, nullptr);

		// Use view without translation so the skybox doesn't move with the camera
		glm::mat4 viewNoTrans = glm::mat4(glm::mat3(activeCamera->GetViewMatrix()));

		sceneData.view = viewNoTrans;

		PushConstants skyPC;
		skyPC.deviceAddress = part.vertexAddress;

		vkCmdPushConstants(cmd, skyPipeline.vkLayout,
		                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		                   0, sizeof(PushConstants), &skyPC);

		part.Draw(cmd, skyPipeline.vkLayout, mat.descriptorSet);
		queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 3);
		sceneStats.drawCallCount++;
	}

	// Don't fetch results here - we just submitted the work! Fetch at BeginFrame after fence wait
	const auto cpuEnd = std::chrono::high_resolution_clock::now();
	sceneStats.cpuDrawTime = static_cast<f32>(std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count());

	renderPass->End(cmd);
}

void Application::ComputeStaticSceneStats()
{
	sceneStats.totalVerts      = 0;
	sceneStats.totalTris       = 0;
	sceneStats.totalMeshCount  = 0;

	for (const auto& [model, transform] : models)
	{
		if (!model) continue;
		for (const auto& part : model->parts)
		{
			if (part.vertexBuffer.buffer != VK_NULL_HANDLE)
				sceneStats.totalVerts += static_cast<u32>(part.vertexBuffer.allocationInfo.size / sizeof(Vertex));
			sceneStats.totalTris += part.indexCount / 3;
			++sceneStats.totalMeshCount;
		}
	}
}


void Application::RenderImGui(const Renderer::FrameContext& frame)
{
	using namespace ImGui;
	VkCommandBuffer cmd = frame.commandContext->commandBuffer;
	TracyVkZone(frame.commandContext->tracyCtx, cmd, "ImGui");


	u32 imageIndex = frame.imageIndex;
	const Extent2D extent = swapchain->GetExtent();
	auto& colorImage = swapchain->images[imageIndex];

	colorImage.Transition(cmd, TextureLayout::ColorWrite);

	Renderer::RenderPassDesc renderPassDesc
	{
		.renderPasses = SPAN_ONE(colorImage), // just the 1 color target
	};

	renderPass->Begin(cmd, extent, renderPassDesc, false);

	// ImGui::ShowDemoWindow();

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (MenuItem("Exit")) { Cleanup(); }
			EditorUI::HoverToolTip("Close this application?");

			ImGui::EndMenu();
		}


		if (ImGui::BeginMenu("View"))
		{

			ImGui::MenuItem("Show GPU Info", nullptr, &showGPUInfo);

			if (ImGui::BeginMenu("Render Views"))
			{
				for (const auto& [value, label] : kDebugViews)
				{
					bool selected = (debugData.debugMode == value);
					if (ImGui::MenuItem(label, nullptr, selected)) debugData.debugMode = value;
					if (selected) ImGui::SetItemDefaultFocus();
				}

				EditorUI::HoverToolTip("Debug Views");
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("VSync"))
			{
				for (const auto& [mode, label] : kVsyncModes)
				{
					const bool selected = (swapchain->presentMode == mode);
					if (ImGui::MenuItem(label, nullptr, selected))
					{
						swapchain->VsyncEnable(mode);
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	const bool showDepthRange =
				debugData.debugMode == DebugView::DepthLin ||
				debugData.debugMode == DebugView::DepthLog;

	enum Corner { Custom = -1, TopLeft, TopRight, BottomLeft, BottomRight, Center };
	static int corner = TopLeft;

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	const ImVec2 work_pos  = vp->WorkPos;
	const ImVec2 work_size = vp->WorkSize;

	// Scale pad with DPI/work area; clamp to reasonable range.
	const float pad = std::clamp(10.0f * vp->DpiScale, 8.0f, 24.0f);

	ImVec2 window_pos;
	ImVec2 window_pivot;

	if (corner == TopLeft)
	{
		window_pos = ImVec2(work_pos.x + pad, work_pos.y + pad);
		window_pivot = ImVec2(0.0f, 0.0f);
	}
	else if (corner == TopRight)
	{
		window_pos = ImVec2(work_pos.x + work_size.x - pad, work_pos.y + pad);
		window_pivot = ImVec2(1.0f, 0.0f);
	}
	else if (corner == BottomLeft)
	{
		window_pos = ImVec2(work_pos.x + pad, work_pos.y + work_size.y - pad);
		window_pivot = ImVec2(0.0f, 1.0f);
	}
	else if (corner == BottomRight)
	{
		window_pos = ImVec2(work_pos.x + work_size.x - pad, work_pos.y + work_size.y - pad);
		window_pivot = ImVec2(1.0f, 1.0f);
	}

	if (corner != Custom)
	{
		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pivot);
		ImGui::SetNextWindowViewport(vp->ID);
	}
	// Base minimum size
	ImVec2 min_size = ImVec2(220.0f, 100.0f);

	if (showDepthRange)
		min_size.x = std::max(min_size.x, 320.0f);
	if (drawAABBs)
		min_size.x = std::max(min_size.x, 420.0f);

	// Expand height for extra rows
	float extra_rows = 0.0f;
	if (showDepthRange)   extra_rows += 1.0f;
	if (drawAABBs) extra_rows += 5.0f;
	min_size.y += extra_rows * ImGui::GetFrameHeightWithSpacing();

	// Max size = screen area minus padding
	ImVec2 max_size = ImVec2(
		std::max(150.0f, work_size.x - pad * 2.0f),
		std::max(80.0f,  work_size.y - pad * 2.0f)
	);

	// Clamp again just to be safe
	min_size.x = std::clamp(min_size.x, 150.0f, max_size.x);
	min_size.y = std::clamp(min_size.y, 80.0f,  max_size.y);

	ImGui::SetNextWindowSizeConstraints(min_size, max_size);
	ImGui::SetNextWindowBgAlpha(0.7f);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoFocusOnAppearing
		| ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoDocking;


	const f32 frameMs = 1000.0f / ImGui::GetIO().Framerate;
	sceneStats.gpuBusy = (frameMs > 0.0f)
			? static_cast<float>((sceneStats.gpuDrawTime / frameMs) * 100.0)
			: 0.0f;
	if (ImGui::Begin("Simple overlay", nullptr, flags))
	{
		// Drag to move only when in Custom mode
		if (corner == Custom)
		{
			ImGui::SetItemAllowOverlap();
			ImGui::SetWindowCollapsed(false);
		}

		ImGui::SeparatorText("Performance");
		{
			// Aligned rows for timings
			if (ImGui::BeginTable("PerfTable", 2, ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("FPS");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f (%.3f ms)", wc->fps, wc->frameTime);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("GPU Draw Time");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f ms", sceneStats.gpuDrawTime);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("GPU Busy");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f%%", sceneStats.gpuBusy);
				EditorUI::HoverToolTip("How much of your frame time the GPU was actively doing work you submitted.");
				ImGui::EndTable();
			}
		}

		ImGui::SeparatorText("Output");
		{
			if (ImGui::BeginTable("OutputTable", 2, ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Resolution");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u x %u", extent.width, extent.height);
				EditorUI::HoverToolTip("Current swapchain/backbuffer resolution");

				ImGui::EndTable();
			}
		}

		ImGui::SeparatorText("Scene Stats");
		{
			if (ImGui::BeginTable("SceneStatsTable", 2, ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Meshes");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", sceneStats.totalMeshCount);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Draw Calls");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", sceneStats.drawCallCount);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Vertices");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", sceneStats.totalVerts);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Triangles");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", sceneStats.totalTris);

				ImGui::EndTable();
			}
		}



		// GPU Info
		if (showGPUInfo)
		{
			ImGui::SeparatorText("GPU Overview");

			if (ImGui::BeginTable("Overview", 2, ImGuiTableFlags_SizingStretchProp))
			{
				// Make first column a little narrower, second stretches
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				// GPU Name
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("GPU");
				ImGui::TableSetColumnIndex(1); ImGui::Text("%s", device->deviceDesc.name.c_str());

				// Driver Version
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Driver Ver");
				const std::string driverVerName = Renderer::VulkanDevice::DecodeDriverVersion(device->deviceDesc.driverVersion,
					device->deviceDesc.vendor);
				ImGui::TableSetColumnIndex(1); ImGui::Text("%s", driverVerName.c_str());

				ImGui::EndTable();
			}
		}

		// Dynamic tables from toggles
		if (ImGui::BeginTable("TimingTable", 2, ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			if (showDepthRange)
			{
				// Section header spans both columns (do two cells on the same row)
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::SeparatorText("Depth Buffer View Options");
				ImGui::TableSetColumnIndex(1);
				ImGui::Dummy(ImVec2(0, 0)); // keep layout happy

				// Depth Range row
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Depth Range");
				ImGui::TableSetColumnIndex(1);
				ImGui::PushItemWidth(-FLT_MIN);
				float range = std::clamp(debugData.debugDepthRange, 1.0f, 1000.0f);
				if (ImGui::SliderFloat("##DepthRange", &range, 1.0f, 1000.0f, "%.1f", ImGuiSliderFlags_Logarithmic))
					debugData.debugDepthRange = range;
				ImGui::PopItemWidth();
			}

			if (drawAABBs)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::SeparatorText("AABB Settings");
				ImGui::TableSetColumnIndex(1);
				ImGui::Dummy(ImVec2(0, 0));

				// Depth bias
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Depth Bias");
				ImGui::TableSetColumnIndex(1);
				ImGui::SliderFloat("##AABBBias", &aabbBias, 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic);

				// Color
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Color");
				ImGui::TableSetColumnIndex(1);
				if (ImGui::ColorButton("##AABBColor", ImVec4(aabbColor.x, aabbColor.y, aabbColor.z, aabbColor.w),
				                       ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar, ImVec2(40, 20)))
				{
					ImGui::OpenPopup("AABBColorPicker");
				}
				if (ImGui::BeginPopup("AABBColorPicker"))
				{
					ImGui::ColorPicker4("##AABBColorPicker", &aabbColor.x,
					                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);
					ImGui::EndPopup();
				}

				// Reset
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (ImGui::Button("Reset"))
				{
					aabbBias = 1e-4f;
					aabbColor = {1.f, 1.f, 0.f, 1.f};
					aabbFlags = 0;
				}
			}

			ImGui::EndTable();
		}

		// Allows it so you can draw it anywhere
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::MenuItem("Top-left",  nullptr, corner == TopLeft))     corner = TopLeft;
			if (ImGui::MenuItem("Top-right", nullptr, corner == TopRight))    corner = TopRight;
			if (ImGui::MenuItem("Bottom-left", nullptr, corner == BottomLeft)) corner = BottomLeft;
			if (ImGui::MenuItem("Bottom-right", nullptr, corner == BottomRight)) corner = BottomRight;
			if (ImGui::MenuItem("Center", nullptr, corner == Center))         corner = Center;
			if (ImGui::MenuItem("Custom", nullptr, corner == Custom))         corner = Custom;
			ImGui::Separator();
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	// EditorUI::DrawInputDebugUI();

	// editorUI->DrawLightingPanel(lights,camera, activeCamera, lightMeta);

	// // root dock + menu + minimize button + default layout handling
	// editorUI->DrawCornerHUD();
	//
	// // draw panels unless globally hidden
	// if (!editorUI->overlayHidden_) {
	// 	const std::string driverVerName = device->DecodeDriverVersion(device->deviceDesc.driverVersion, device->deviceDesc.vendor).c_str();
	// 	editorUI->DrawSceneStatsPanel(extent, device->deviceDesc.name, driverVerName, sceneStats, models, *activeCamera);
	// 	editorUI->DrawInputDebugUI(input);
	// 	editorUI->DrawLightingPanel(lights, camera, activeCamera, lightMeta);
	// 	editorUI->DrawRenderingPanel(swapchain, debugData.debugMode);
	// }

	EditorUI::EndFrame();
	EditorUI::Render(cmd);

	renderPass->End(cmd);
	colorImage.Transition(cmd, TextureLayout::Present);
}

void Application::Run()
{
	while (Platform::ProcessMessages())
	{
		FrameMarkStart("Frame");
		ZoneScopedN("Frame");
		{
			ZoneScopedN("GameInput Update");
			gameInput.Update();
		}

		Platform::StartFrame(*wc);
		if (renderer->ResizeIfNeeded()) { FrameMarkEnd("Frame"); continue; }
		
		auto frame = renderer->BeginFrame();
		if (frame.commandContext == nullptr) // Failed to acquire image (window closing, resize, etc.)
		{
			FrameMarkEnd("Frame");
			continue; // Skip this frame gracefully
		}
		
		EditorUI::BeginFrame();

		UpdateCamera();
		UpdateSceneUBO(frame);

		RenderScene(frame);
		RenderImGui(frame);

		renderer->EndFrame(frame);
		FrameMarkEnd("Frame");
	}
	
	// CRITICAL: Ensure GPU is idle before cleanup
	if (device && device->device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(device->device);
	}
}

void Application::Cleanup()
{
	// CRITICAL: Wait for all GPU work to finish before destroying anything
	if (device && device->device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(device->device);
	}

	// 1) Destroy pipelines FIRST (they reference descriptor set layouts in sceneUBO and model layouts)
	if (scenePipeline) scenePipeline.Destroy();
	if (skyPipeline)   skyPipeline.Destroy();
	if (aabbPipeline)  aabbPipeline.Destroy();

	// 2) Destroy shader modules (if not auto-destroyed by arena)
	if (shader)     { shader->Destroy();     shader = nullptr; }
	if (skyShader)  { skyShader->Destroy();  skyShader = nullptr; }
	if (aabbShader) { aabbShader->Destroy(); aabbShader = nullptr; }

	// 3) Destroy descriptor-backed resources next (their layouts are no longer referenced by pipelines)
	if (modelInst) modelInst->Destroy(device);   // frees model layout inside
	if (cubeMesh)  cubeMesh->Destroy(device);
	if (skyModel)  skyModel->Destroy(device);    // frees sky material layout

	// 4) Destroy images/samplers used by materials and fallbacks
	if (skyCubeMap.image)  skyCubeMap.Destroy();
	skySampler.Destroy();

	checkerboardImage.Destroy();
	if (checkerboardSampler) checkerboardSampler->Destroy();
	normalFallbackImage.Destroy(); // was previously leaked

	// 5) UBO & allocators (was used by pipelines; safe to destroy now)
	if (sceneUBO) { sceneUBO->Destroy(); sceneUBO = nullptr; }

	// descriptor pools
	if (globalDescriptorAlloc) globalDescriptorAlloc->DestroyPools();

	// 6) Renderer/UI/Swapchain/Device/Instance (broad backend teardown)
	if (editorUI)  editorUI->Destroy();
	if (renderer)  renderer->Destroy();
	if (swapchain) swapchain->Destroy();

	if (device)   device->Destroy();
	if (instance) instance->Destroy();
}
