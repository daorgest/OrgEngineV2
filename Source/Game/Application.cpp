//
// Created by Orgest on 7/13/2025.
//

#include "Application.h"

#include <algorithm>
#include <chrono>

#include "MathFuncs.h"
#include "MeshData.h"
#include "MeshLoader.h"
#include "MeshStats.h"
#include "TextureManager.h"
#include "Timer.h"
#include "VulkanPipeline.h"
#include "Input/InputSys.h"
#include "tracy/Tracy.hpp"

bool Application::Init()
{
	ZoneScopedN("Application::Init");
	Log::InitLogFile();

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
		Array<Renderer::DescriptorAllocatorGrowable::PoolSizeRatio, 5> sizes{
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 4},
		    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
		};
		globalDescriptorAlloc->Init(device, 4, sizes);
	}

	{
		ZoneScopedN("Init Renderer & UI");
		renderer->Init(device, swapchain);
		editorUI->Init(instance, device, swapchain);
	}

	{
		ZoneScopedN("Init Scene UBO");
		UniformBufferDesc sceneDesc = {
			.framesInFlight = MAX_FRAME_OVERLAP,
			.stageFlags = ShaderStage::ALL_GRAPHICS,
			.debugName = "Scene UBO",
			.bindings = {
				{2, DescriptorType::UniformBuffer, sizeof(DebugUBO)},
				{3, DescriptorType::UniformBuffer, sizeof(CameraUBO)},
				{4, DescriptorType::StorageBuffer, sizeof(LightUBO) * 8},
				{5, DescriptorType::UniformBuffer, sizeof(LightMeta)},
			}
		};
		sceneUBO = coreArena.Emplace<Renderer::VulkanShaderBuffer>(device, globalDescriptorAlloc, sceneDesc);
	}

	SamplerDesc samplerDesc{};
	samplerDesc.minFilter = SamplerFilter::Nearest;
	samplerDesc.magFilter = SamplerFilter::Nearest;
	samplerDesc.mipFilter = SamplerMipFilter::None;

	checkerboardImage = Renderer::VulkanImage::CreateCheckerboardTexture(device, renderArena);
	checkerboardSampler = coreArena.Emplace<Renderer::VulkanSampler>(device, samplerDesc);
	Renderer::TextureFallback fallback {.fallbackImage = checkerboardImage, .fallbackSampler = checkerboardSampler};

	{
		ZoneScopedN("LoadModel");
		Timer time("Model Source Loading");
		auto result = Assets::MeshLoader::LoadModelFromSource(MeshSourceType::OBJ, "orgmodel/Flopkin_Chub_Edit.obj");
		if (!result)
		{
			LOG(Error, "Failed to load model: {}", result.error());
			return false;
		}


		model = std::make_unique<Renderer::VulkanModel>();

		// Convert to GPU model
		Timer time2("Model Loading");
		model->LoadModel(device, *result, &fallback);

		Renderer::VulkanModelComponent comp{};
		comp.model = model.get();
		comp.transform = Mat4x4::Identity(); // or Translation({x,y,z}) if you want it offset
		models.push_back(comp);

	}

	sceneUBO->AllocateDescriptorSets();

	const Array cubeVertices = {
		// Front face (+Z)
		Vertex{{-0.5f,-0.5f, 0.5f}, { 0,  0, 1}, {1,0,0}, {0,0}},
		Vertex{{ 0.5f,-0.5f, 0.5f}, { 0,  0, 1}, {0,1,0}, {1,0}},
		Vertex{{ 0.5f, 0.5f, 0.5f}, { 0,  0, 1}, {0,0,1}, {1,1}},
		Vertex{{-0.5f, 0.5f, 0.5f}, { 0,  0, 1}, {1,1,1}, {0,1}},

		// Back face (-Z)
		Vertex{{ 0.5f,-0.5f,-0.5f}, { 0,  0,-1}, {1,1,0}, {0,0}},
		Vertex{{-0.5f,-0.5f,-0.5f}, { 0,  0,-1}, {0,1,1}, {1,0}},
		Vertex{{-0.5f, 0.5f,-0.5f}, { 0,  0,-1}, {1,0,1}, {1,1}},
		Vertex{{ 0.5f, 0.5f,-0.5f}, { 0,  0,-1}, {0.5f,0.5f,0.5f}, {0,1}},

		// Right face (+X)
		Vertex{{ 0.5f,-0.5f, 0.5f}, { 1,  0, 0}, {1,0,0}, {0,0}},
		Vertex{{ 0.5f,-0.5f,-0.5f}, { 1,  0, 0}, {0,1,0}, {1,0}},
		Vertex{{ 0.5f, 0.5f,-0.5f}, { 1,  0, 0}, {0,0,1}, {1,1}},
		Vertex{{ 0.5f, 0.5f, 0.5f}, { 1,  0, 0}, {1,1,1}, {0,1}},

		// Left face (-X)
		Vertex{{-0.5f,-0.5f,-0.5f}, {-1,  0, 0}, {1,1,0}, {0,0}},
		Vertex{{-0.5f,-0.5f, 0.5f}, {-1,  0, 0}, {0,1,1}, {1,0}},
		Vertex{{-0.5f, 0.5f, 0.5f}, {-1,  0, 0}, {1,0,1}, {1,1}},
		Vertex{{-0.5f, 0.5f,-0.5f}, {-1,  0, 0}, {0.5f,0.5f,0.5f}, {0,1}},

		// Top face (+Y)
		Vertex{{-0.5f, 0.5f, 0.5f}, { 0,  1, 0}, {1,0,0}, {0,0}},
		Vertex{{ 0.5f, 0.5f, 0.5f}, { 0,  1, 0}, {0,1,0}, {1,0}},
		Vertex{{ 0.5f, 0.5f,-0.5f}, { 0,  1, 0}, {0,0,1}, {1,1}},
		Vertex{{-0.5f, 0.5f,-0.5f}, { 0,  1, 0}, {1,1,1}, {0,1}},

		// Bottom face (-Y)
		Vertex{{-0.5f,-0.5f,-0.5f}, { 0, -1, 0}, {1,1,0}, {0,0}},
		Vertex{{ 0.5f,-0.5f,-0.5f}, { 0, -1, 0}, {0,1,1}, {1,0}},
		Vertex{{ 0.5f,-0.5f, 0.5f}, { 0, -1, 0}, {1,0,1}, {1,1}},
		Vertex{{-0.5f,-0.5f, 0.5f}, { 0, -1, 0}, {0.5f,0.5f,0.5f}, {0,1}},
	};

	const Array<u32, 36> cubeIndices = {
		0,1,2, 2,3,0,    // Front
		4,5,6, 6,7,4,    // Back
		8,9,10, 10,11,8, // Right
		12,13,14, 14,15,12, // Left
		16,17,18, 18,19,16, // Top
		20,21,22, 22,23,20  // Bottom
	};


	// its fine (iits not) that i kinda want it to be views instead
	MeshPart cubePart =
	{
		Vector<Vertex>(cubeVertices.begin(), cubeVertices.end()),
		Vector<u32>(cubeIndices.begin(), cubeIndices.end()),
	};

	cubeMesh = std::make_unique<Renderer::VulkanModel>();
	cubeMesh->parts.emplace_back();
	cubeMesh->parts.back().Create(device, cubePart);

	cubeMesh->layout = models[0].model->layout;

	Array<Renderer::DescriptorAllocatorGrowable::PoolSizeRatio, 2> sizes = {
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1.f },
		{ VK_DESCRIPTOR_TYPE_SAMPLER,       1.f }
	};
	cubeMesh->descriptorPool.Init(device, 1, sizes);

	cubeMesh->materials.clear();
	cubeMesh->materials.emplace_back();
	Renderer::VulkanMaterial& cubeMat = cubeMesh->materials.back();
	cubeMat.colorImage    = checkerboardImage;
	cubeMat.sampler       = checkerboardSampler;
	cubeMat.materialIndex = 0;
	cubeMat.descriptorSet = cubeMesh->descriptorPool.Allocate(cubeMesh->layout);

	Renderer::VkDescriptorWriter w;
	w
	 .WriteImage(0, cubeMat.colorImage, nullptr,      DescriptorType::SampledImage)
	 .WriteImage(1, std::nullopt,       cubeMat.sampler, DescriptorType::Sampler)
	 .UpdateSet(device->device, cubeMat.descriptorSet);

	cubeMesh->parts.back().materialIndex = 0;

	Renderer::VulkanModelComponent cubeComp{};
	cubeComp.model = cubeMesh.get();
	cubeComp.transform = Mat4x4::Translation({ 2.0f, 0.0f, 0.0f });
	models.push_back(cubeComp);


	Vector<u32> code = Renderer::VulkanShader::ReadShaderFile("Shaders/scene.spv");
	shader = renderArena.Emplace<Renderer::VulkanShader>(device, std::span<const u32>(code));
	// auto* shader = coreArena.Emplace<Renderer::VulkanShader>(device, SCENE_SPV, SCENE_SPV_SIZE, ShaderFormat::SPIRV);

	// I gotta abstract this..
	VkDescriptorSetLayout setLayouts[] = {
		sceneUBO->layout,                      // set = 0
		models[0].model->layout				   // set = 1 (all materials share same layout)
	};

	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset     = 0,
		.size       = sizeof(RenderDrawPushConstants)
	};

	VkPipelineLayoutCreateInfo layoutInfo = {
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = static_cast<u32>(std::size(setLayouts)),
		.pSetLayouts            = setLayouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &pushConstantRange
	};

	VK_CHECK(vkCreatePipelineLayout(device->device, &layoutInfo, nullptr, &pipelineLayout));


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
		.Layout(pipelineLayout);
	pipeline = builder.BuildPipeline(device->device);

	camera.position = { 1.77f, 1.21f, -1.70f };
	camera.forward  = Vec3{ -0.71f, -0.17f, 0.69f }.Normalized();
	camera.up       = { 0.0f, 1.0f, 0.0f };

	auto cosd = [](float deg){ return std::cos(Radians(deg)); };

	lights.clear();

	// #0: Directional “sun”
	{
		LightUBO L{};
		L.type      = (u32)LightType::Directional;
		L.direction = Vec3{-0.5f, -1.0f, -0.3f}.Normalized();
		L.color     = Vec3{1.0f, 0.95f, 0.85f};
		L.intensity = 2.0f;
		lights.push_back(L);
	}

	// initialize meta
	lightMeta.count = static_cast<u32>(lights.size());
	debugData.debugMode = DebugView::Material;

	// TextureInfo offscreenViewportInfo = {
	// 	.extent = { swapchain->width, swapchain->height, 1 },
	// 	.format = TextureFormat::RGBA8_UNORM,
	// 	.usage = ImageUsage::COLOR_ATTACHMENT | ImageUsage::SAMPLED
	// };
	//
	// offscreenImage = coreArena.Emplace<Renderer::VulkanImage>(device, offscreenViewportInfo);

	return true;
}

void Application::UpdateCamera()
{
	ZoneScopedN("Camera Update");
	Vec3 movement{0, 0, 0};
	const auto& dt = wc->GetDeltaTime();


	constexpr f32 MOUSE_SENSITIVITY = 0.5f; // Tweak as needed
	constexpr f32 PAN_SENSITIVITY = 0.5f; // Tweak as needed
	if (input.mouseLookActive && input.mouseButtons[Mouse::Left].held)
	{
		camera.yaw   -= input.xrel * MOUSE_SENSITIVITY;
		camera.pitch -= input.yrel * MOUSE_SENSITIVITY;
		camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);

		camera.UpdateDirectionVectors();
		input.xrel = 0.0f;
		input.yrel = 0.0f;
	}


	if (input.keyboard[Keyboard::W].held) movement += camera.forward;
	if (input.keyboard[Keyboard::S].held) movement -= camera.forward;
	if (input.keyboard[Keyboard::A].held) movement -= camera.right;
	if (input.keyboard[Keyboard::D].held) movement += camera.right;
	if (input.keyboard[Keyboard::Q].held) movement -= camera.up;
	if (input.keyboard[Keyboard::E].held) movement += camera.up;

	// Pan
	if (input.mouseButtons[Mouse::Middle].held && input.keyboard[Keyboard::Alt].held)
	{
		camera.position += camera.right * (-input.xrel * PAN_SENSITIVITY * dt);
		camera.position += camera.up    * ( input.yrel * PAN_SENSITIVITY * dt);

		input.xrel = 0.0f;
		input.yrel = 0.0f;
	}

	if (movement.LengthSquared() > 0.0f)
		movement = movement.Normalized();


	camera.position += movement * dt * cameraSpeed;

	// camera speed handling
	const ImGuiIO& io = ImGui::GetIO();
	input.mouseLookActive = !io.WantCaptureMouse;
	if (input.scrollDelta != 0.0f && input.mouseLookActive)
	{
		constexpr float scrollStep = 0.5f;
		cameraSpeed += input.scrollDelta * scrollStep;
		cameraSpeed = std::clamp(cameraSpeed, 0.1f, 20.0f); // safer range
		cameraSpeedPopupTime = 1.5f;
	}

	input.scrollDelta = 0;
	if (cameraSpeedPopupTime > 0.0f)
		cameraSpeedPopupTime -= dt;

	Input::EndFrameInputUpdate();
}

void Application::UpdateSceneUBO(const Renderer::FrameContext& frame)
{
	ZoneScopedN("UpdateSceneUBO");

	aspectRatio = static_cast<float>(swapchain->width) / static_cast<float>(swapchain->height);
	sceneData.model = Mat4x4::Identity();
	sceneData.view  = camera.GetViewMatrix();
	sceneData.proj  = camera.GetProjectionMatrix(aspectRatio);

	camUBO.position   = camera.position;
	camUBO.nearPlane  = camera.nearPlane;
	camUBO.farPlane   = camera.farPlane;

	// keep flashlight glued to camera if present as #1
	if (lights.size() >= 2 && lights[1].type == LightType::Spot) {
		lights[1].position  = camera.position;
		lights[1].direction = camera.forward;
	}

	lightMeta.count = (u32)lights.size();


	const u32 frameIndex = frame.frameIndex;
	sceneUBO->UpdateBinding(frameIndex, 2, &debugData, sizeof(DebugUBO));
	sceneUBO->UpdateBinding(frameIndex, 3, &camUBO, sizeof(CameraUBO));
	sceneUBO->UpdateBinding(frameIndex, 4, lights.data(), sizeof(LightUBO) * lights.size());
	sceneUBO->UpdateBinding(frameIndex, 5, &lightMeta, sizeof(LightMeta));
}


void Application::RenderScene(const Renderer::FrameContext& frame)
{
	ZoneScopedN("RenderScene");

	VkCommandBuffer cmd = frame.commandContext->commandBuffer;
	const u32 imageIndex = frame.imageIndex;
	const Extent2D extent = swapchain->GetExtent();

	auto& colorImage = swapchain->images[imageIndex];
	auto& depthImage = swapchain->depthImage;
	const auto& queryPool = frame.frameData->queryPool;

	// Transition attachments
	colorImage.Transition(cmd, TextureLayout::ColorWrite);
	depthImage.Transition(cmd, TextureLayout::DepthWrite);

	// depthImage.Transition(cmd, TextureLayout::DepthReadOnly);
	// Begin render pass
	// just before Scene Begin(...)
	renderPass->clearValues.clear();
	renderPass->clearValues.push_back({ .color = {{0.f,0.f,0.f,1.f}} }); // color clear (black)
	renderPass->clearValues.push_back({ .depthStencil = {0.0f, 0} });    // reverse-Z depth clear
	renderPass->depthStencilAttachment = depthImage.imageView;
	renderPass->Begin(cmd, extent, colorImage.imageView, /*clear=*/true);

	// Set viewport, bind pipeline and UBO
	Renderer::VulkanRenderer::SetViewportAndScissor(cmd, extent);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	sceneUBO->Bind(cmd, pipelineLayout, frame.frameIndex, 0);

	// Start scene timing
	sceneStats.drawCallCount = 0;
	const auto cpuStart = std::chrono::high_resolution_clock::now();
	queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0);

	for (const auto& inst : models)
	{
		const Mat4x4 modelMatrix = inst.transform;
		Renderer::VulkanModel* mdl = inst.model;
		if (!mdl) continue;

		for (const auto& part : mdl->parts)
		{
			const u32 materialIndex = part.materialIndex;
			if (materialIndex >= mdl->materials.size()) continue;

			const Renderer::VulkanMaterial& mat = mdl->materials[materialIndex];

			RenderDrawPushConstants pc;
			pc.worldMatrix  = sceneData.proj * sceneData.view * modelMatrix;
			pc.deviceAddress = part.vertexAddress;

			vkCmdPushConstants(cmd, pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(RenderDrawPushConstants), &pc);

			part.Draw(cmd, pipelineLayout, mat.descriptorSet);
			sceneStats.drawCallCount++;
		}
	}


	queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1);

	const auto cpuEnd = std::chrono::high_resolution_clock::now();
	sceneStats.cpuDrawTime = static_cast<f32>(std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count());

	// Fetch GPU draw time if available
	if (queryPool.queryResults[0].available && queryPool.queryResults[1].available)
	{
		u64 start = queryPool.queryResults[0].time;
		u64 end   = queryPool.queryResults[1].time;

		if (end > start)
			sceneStats.gpuDrawTime = static_cast<f32>(end - start) * 1e-6f;
	}
	renderPass->End(cmd);
}

void Application::RenderImGui(const Renderer::FrameContext& frame)
{
	ZoneScopedN("RenderImGui");
	VkCommandBuffer cmd = frame.commandContext->commandBuffer;
	u32 imageIndex = frame.imageIndex;
	const Extent2D extent = swapchain->GetExtent();

	auto& colorImg = swapchain->images[imageIndex];

	colorImg.Transition(cmd, TextureLayout::ColorWrite);

	renderPass->depthStencilAttachment.reset();
	renderPass->Begin(cmd, extent, colorImg.imageView, false);
	if (ImGui::Begin("Lighting"))
	{
		ImGui::Text("Lights: %u", (u32)lights.size());
		ImGui::Separator();

		// Quick add buttons
		if (ImGui::Button("+ Directional"))
		{
			LightUBO L{};
			L.type = (u32)LightType::Directional;
			L.direction = Vec3{-0.5f, -1.0f, -0.3f}.Normalized();
			L.color = {1.0f, 0.95f, 0.85f};
			L.intensity = 2.0f;
			lights.push_back(L);
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Point"))
		{
			LightUBO L{};
			L.type = (u32)LightType::Point;
			L.position = camera.position + camera.forward * 2.0f;
			L.range = 8.0f;
			L.color = {1, 1, 1};
			L.intensity = 5.0f;
			lights.push_back(L);
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Spot"))
		{
			LightUBO L{};
			L.type = (u32)LightType::Spot;
			L.position = camera.position;
			L.direction = camera.forward.Normalized();
			L.range = 12.0f;
			L.innerCone = std::cos(Radians(12.0f));
			L.outerCone = std::cos(Radians(20.0f));
			L.color = {1, 1, 1};
			L.intensity = 6.0f;
			lights.push_back(L);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) lights.clear();

		ImGui::Separator();

		// Bigger, nicer steps
		constexpr float kDirStep = 0.05f;
		constexpr float kPosStep = 0.05f;
		constexpr float kIntensityStep = 0.25f;
		constexpr float kRangeStep = 0.5f;

		static const char* kTypes[] = {"Directional", "Point", "Spot"};

		for (size_t i = 0; i < lights.size();)
		{
			ImGui::PushID(static_cast<int>(i));
			ImGui::SeparatorText(("Light " + std::to_string(i)).c_str());
			LightUBO& L = lights[i];

			int type = (int)L.type;
			if (ImGui::Combo("Type", &type, kTypes, IM_ARRAYSIZE(kTypes)))
			{
				L.type = (u32)type;
			}

			ImGui::ColorEdit3("Color", &L.color.x);
			ImGui::DragFloat("Intensity", &L.intensity, kIntensityStep, 0.0f, 1000.0f);

			if (L.type == (u32)LightType::Directional)
			{
				if (ImGui::DragFloat3("Direction", &L.direction.x, kDirStep, -1.0f, 1.0f))
				{
					float len2 = L.direction.x * L.direction.x + L.direction.y * L.direction.y + L.direction.z * L.direction.z;
					if (len2 > 1e-6f) L.direction = L.direction / std::sqrt(len2);
				}
				if (ImGui::Button("Face Camera"))
				{
					L.direction = camera.forward;
				}
			}
			else if (L.type == (u32)LightType::Point)
			{
				ImGui::DragFloat3("Position", &L.position.x, kPosStep);
				ImGui::DragFloat("Range", &L.range, kRangeStep, 0.0f, 1000.0f);
			}
			else // Spot
			{
				ImGui::DragFloat3("Position", &L.position.x, kPosStep);
				if (ImGui::DragFloat3("Direction", &L.direction.x, kDirStep, -1.0f, 1.0f))
				{
					float len2 = L.direction.x * L.direction.x + L.direction.y * L.direction.y + L.direction.z * L.direction.z;
					if (len2 > 1e-6f) L.direction = L.direction / std::sqrt(len2);
				}
				ImGui::SameLine();
				if (ImGui::Button("Snap To Camera"))
				{
					L.position = camera.position;
					L.direction = camera.forward;
				}

				ImGui::DragFloat("Range", &L.range, kRangeStep, 0.1f, 1000.0f);

				// --- Cones: edit in degrees via SliderAngle, store cosines ---
				auto clamp1 = [](float v) { return std::max(-1.0f, std::min(1.0f, v)); };

				float innerRad = std::acos(clamp1(L.innerCone)); // radians
				float outerRad = std::acos(clamp1(L.outerCone)); // radians

				bool changed = false;
				// inner: 0..80 deg
				changed |= ImGui::SliderAngle("Inner", &innerRad, 0.0f, 80.0f, "%.1f°");
				// outer: at least inner + 1°, up to 89°
				float minSep = Radians(1.0f);
				float outerMin = innerRad + minSep;
				changed |= ImGui::SliderAngle("Outer", &outerRad, outerMin, 89.0f, "%.1f°");

				if (changed)
				{
					L.innerCone = std::cos(innerRad);
					L.outerCone = std::cos(outerRad);
				}
			}

			if (ImGui::Button("Remove"))
			{
				lights.erase(vecSizeType((lights.begin() + i)));
				ImGui::PopID();
				continue;
			}
			++i;
			ImGui::PopID();
		}

		// keep meta in sync
		lightMeta.count = (u32)lights.size();
	}
	ImGui::End();

	if (ImGui::Begin("Scene Stats"))
	{
		// Resolution and FPS
		ImGui::Text("Resolution: %ux%u", extent.width, extent.height);
		ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
		ImGui::Separator();

		// Camera Info
		ImGui::Text("Camera Info:");
		ImGui::BulletText("Position: (%.2f, %.2f, %.2f)", camera.position.x, camera.position.y, camera.position.z);
		ImGui::BulletText("Forward:  (%.2f, %.2f, %.2f)", camera.forward.x, camera.forward.y, camera.forward.z);
		ImGui::BulletText("FOV: %.2f", camera.fov);
		ImGui::BulletText("Near/Far: %.2f / %.2f", camera.nearPlane, camera.farPlane);

		ImGui::Separator();

		u32 totalVerts = 0;
		u32 totalTris  = 0;
		u32 totalDraws = 0;

		for (const auto& inst : models)
		{
			const auto* mdl = inst.model;
			if (!mdl) continue;

			for (const auto& part : mdl->parts)
			{
				// verts (from VB size)
				if (part.vertexBuffer.buffer)
				{
					// pointer or unique_ptr
					totalVerts += static_cast<u32>(part.vertexBuffer.allocationInfo.size / sizeof(Vertex));
				}
				// tris
				totalTris += part.indexCount / 3;
				// draw call per part
				++totalDraws;
			}
		}

		ImGui::Text("Scene Stats:");
		ImGui::BulletText("Draw Calls: %u", sceneStats.drawCallCount);
		ImGui::BulletText("Vertices:   %u", totalVerts);
		ImGui::BulletText("Triangles:  %u", totalTris);

		ImGui::Separator();

		// Timing
		const double gpuBusy = std::clamp((sceneStats.gpuDrawTime / (1000.0 / ImGui::GetIO().Framerate)) * 100.0, 0.0, 100.0);
		ImGui::Text("Timing:");
		ImGui::BulletText("CPU Draw Time:  %.3f ms", sceneStats.cpuDrawTime);
		ImGui::BulletText("GPU Draw Time:  %.3f ms", sceneStats.gpuDrawTime);
		ImGui::BulletText("GPU Busy:   %.1f %%", gpuBusy);

		ImGui::Separator();

		// Debug View Mode
		static const char* modes[] = { "None(same as Lighting)", "Albedo", "Normals", "Depth", "Material" };
		int selected = static_cast<int>(debugData.debugMode);
		if (ImGui::Combo("Debug View", &selected, modes, IM_ARRAYSIZE(modes)))
			debugData.debugMode = static_cast<DebugView>(selected);

		// VSync Mode
		static const char* vsyncModes[] = { "VSync On", "VSync Off", "Mailbox" };
		int currentVsync = static_cast<int>(swapchain->presentMode);
		if (ImGui::Combo("VSync Mode", &currentVsync, vsyncModes, IM_ARRAYSIZE(vsyncModes)))
			swapchain->VsyncEnable(static_cast<PresentMode>(currentVsync));
	}
	ImGui::End();

	if (cameraSpeedPopupTime > 0.0f)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImVec2 pos = ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.12f);

		const float alpha = std::min(1.0f, cameraSpeedPopupTime / 1.0f);
		ImGui::SetNextWindowBgAlpha(alpha);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoInputs;

		if (ImGui::Begin("##CameraSpeedPopup", nullptr, flags))
		{
			ImGui::Text("Speed: %.2f", cameraSpeed);
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	editorUI->DrawCameraGizmo(&camera);
	editorUI->EndFrame();
	editorUI->Render(cmd);

	renderPass->End(cmd);
	colorImg.Transition(cmd, TextureLayout::Present);
}

void Application::Run()
{
	while (Platform::ProcessMessages())
	{
		ZoneScopedN("Frame");

		Platform::StartFrame(*wc);
		if (renderer->ResizeIfNeeded()) continue;
		if (swapchain->needsRecreation)
		{
			swapchain->Recreate();
			swapchain->needsRecreation = false;
		}
		auto frame = renderer->BeginFrame();
		editorUI->BeginFrame();

		if (!frame.commandContext) continue;

		UpdateCamera();
		UpdateSceneUBO(frame);

		RenderScene(frame);
		RenderImGui(frame);

		renderer->EndFrame(frame);
	}
}

void Application::Cleanup()
{
	vkDeviceWaitIdle(device->device);
	renderer->Destroy();
	editorUI->Destroy();
	model->Destroy(device);
	cubeMesh->Destroy(device);

	checkerboardImage->Destroy();
	checkerboardSampler->Destroy();
	sceneUBO->Destroy();
	vkDestroyPipeline(device->device, pipeline, nullptr);
	vkDestroyPipelineLayout(device->device, pipelineLayout, nullptr);
	shader->Destroy();

	swapchain->Destroy();
	globalDescriptorAlloc->DestroyPools();

	device->Destroy();
	instance->Destroy();
}
