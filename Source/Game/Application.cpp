//
// Created by Orgest on 7/13/2025.
//

#include "Application.h"

#include <algorithm>
#include <chrono>

#include "DebugRenderer.h"
#include "EditorUi.h"
#include "imgui.h"
#include "MeshLoader.h"
#include "SkyboxManager.h"
#include "VulkanPipeline.h"
#include "../Core/Tools/Logger.h"
#include "../Engine/MeshData.h"
#include "../Engine/MeshGenerator.h"
#include "../Engine/MeshStats.h"
#include "backends/imgui_impl_vulkan.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"
#include "Input/InputSys.h"
#include "Input/InputSysGameInput.h"
#include "Tools/DeletionQueue.h"
#include "tracy/Tracy.hpp"

void Application::DrawLoadingSplash(const char* text) const
{
	// Process a frame just like Run() does, but only once.
	if (renderer->ResizeIfNeeded()) { return; }
	if (swapchain->needsRecreation)
	{
		swapchain->Recreate();
	}

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
	VkCommandBuffer cmd = static_cast<Renderer::VulkanCommandBuffer*>(frame.commandContext)->GetVkHandle();

	auto& colorImage = swapchain->images[frame.imageIndex];
	colorImage.Transition(cmd, TextureLayout::ColorWrite);

	const Renderer::RenderPassDesc rpDesc = {
		.renderPasses = SPAN_ONE(colorImage),
	};

	renderPass->Begin(cmd, swapchain->GetExtent(), rpDesc, true);

	editorUI->EndFrame();
	editorUI->Render(cmd);

	renderPass->End(cmd);
	colorImage.Transition(cmd, TextureLayout::Present);

	renderer->EndFrame(frame);
}


bool Application::Init()
{
	ZoneScopedN("Application::Init");
	Log::InitLogFile();

	{
		ZoneScopedN("Init Platform & Window");
		windowContext = coreArena.Emplace<Platform::WindowContext>();
		instance = coreArena.Emplace<Renderer::VulkanInstance>();
		device = coreArena.Emplace<Renderer::VulkanDevice>();
		swapchain = coreArena.Emplace<Renderer::VulkanSwapchain>();
		renderer = coreArena.Emplace<Renderer::VulkanRenderer>();
		renderPass = coreArena.Emplace<Renderer::VulkanRenderPass>();
		globalDescriptorAlloc = coreArena.Emplace<Renderer::DescriptorAllocatorGrowable>();
		editorUI = coreArena.Emplace<EditorUI>();
		Platform::Init(windowContext);
		// Platform::ShowWindow(*wc);
	}

	{
		ZoneScopedN("Init Vulkan Instance, Device, Swapchain, Editor UI, and Command Buffers");
		instance->Init();
		device->Init(instance);
		swapchain->Init(device, windowContext->handle);
		renderer->Init(device, swapchain);

		editorUI->state.wc            = windowContext;
		editorUI->state.device        = device;
		editorUI->state.swapchain     = swapchain;
		editorUI->state.debugRenderer = &debugRenderer;
		editorUI->state.sceneStats    = &sceneStats;
		editorUI->state.lights        = &lights;
		editorUI->state.debugData     = &debugData;
		editorUI->state.cameraSpeed   = cameraSpeed;

		editorUI->Init(instance, device, swapchain);
	}
	DrawLoadingSplash("Loading...");

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
		ZoneScopedN("Init Scene UBO");
		UniformBufferDesc sceneDesc = {
			.setIndex = 0,
			.stageFlags = ShaderStage::AllGraphics,
			.bindings = {
				{2, DescriptorType::UniformBuffer, sizeof(DebugUBO)},
				{3, DescriptorType::UniformBuffer, sizeof(CameraUBO)},
				{4, DescriptorType::StorageBuffer, sizeof(LightUBO) * 8}, // ew
				{5, DescriptorType::UniformBuffer, sizeof(LightUBOCount)},
				{6, DescriptorType::UniformBuffer, sizeof(SceneUBO)},
			}
		};
		sceneUBO = coreArena.Emplace<Renderer::VulkanShaderBuffer>(device, globalDescriptorAlloc, sceneDesc);
		sceneUBO->AllocateDescriptorSets();
	}

	// Checkerboard sampler
	SamplerDesc samplerDesc{};
	samplerDesc.minFilter = SamplerFilter::Nearest;
	samplerDesc.magFilter = SamplerFilter::Nearest;
	samplerDesc.mipFilter = SamplerMipFilter::None;

	checkerboardImage = Renderer::VulkanImage::CreateCheckerboardTexture(*device);
	checkerboardSampler = coreArena.Emplace<Renderer::VulkanSampler>(device, samplerDesc);

	// Normal map fallback
	SamplerDesc normalSamplerDesc{};
	normalFallbackSampler = coreArena.Emplace<Renderer::VulkanSampler>(device, normalSamplerDesc);
	normalFallbackImage = Renderer::VulkanImage::CreateDefaultNormalMap(*device);

	// Create white 1x1 texture for PBR spheres
	{
		TextureInfo whiteTexInfo{
			.extent = {1, 1, 1},
			.mipLevels = 1,
			.type = ImageType::Image2D,
			.format = TextureFormat::RGBA8_SRGB,
			.dimension = TextureDimension::Texture2D,
			.usage = ImageUsage::Sampled | ImageUsage::TransferDst,
		};
		whiteImage = Renderer::VulkanImage(device, whiteTexInfo);
		u32 whitePixel = 0xFFFFFFF; // RGBA white
		whiteImage.UploadTextureToGPU(&whitePixel, whiteTexInfo);
	}

	// Renderer::TextureOrFallback fallback = {
	// 	.fallbackImage = &checkerboardImage,
	// 	.fallbackSampler = checkerboardSampler,
	// 	.fallbackNormalImage = &normalFallbackImage,
	// };
	//
	// // Main model
	// {
	// 	ZoneScopedN("LoadModel From Source!");
	// 	DrawLoadingSplash("Loading OBJ Model...");
	// 	LOG(Debug, "Loading main model: Sponza/sponza.obj");
	//
	// 	auto result = Assets::MeshLoader::LoadModelFromSource(MeshSourceType::OBJ, "Bistro/Bostro.obj");
	// 	modelInst = renderArena.Emplace<Renderer::VulkanModel>(device, *result, fallback);
	// 	Renderer::ModelComponent comp = {
	//
	// 		.model = modelInst,  // Already a raw pointer now
	// 		.transform = glm::scale(glm::vec3(0.01f))
	// 	};
	// 	models.push_back(comp);
	// }

	DrawLoadingSplash("Building Skybox...");

	if (!skybox.Initialize(device, &renderArena))
	{
		LOG(Warning, "Skybox initialization failed - skybox will not be rendered");
	}

	DrawLoadingSplash("Creating PBR Sphere Grid...");

	CreatePBRSphereGrid();

	DrawLoadingSplash("Compiling Shaders...");
	auto codeResult = Renderer::VulkanShader::ReadShaderFile("Shaders/scene.spv");
	if (!codeResult)
	{
		LOG(Error, "Failed to load shader: Shaders/scene.spv ({})", static_cast<int>(codeResult.error()));
		return false;
	}

	CHECK_RESULT(sceneShader.Init(device, codeResult.value()));

	// Scene pipeline descriptor sets:
	// Set 0: Scene UBO (camera, lights, debug)
	// Set 1: Material (albedo, normal textures)
	// Set 2: Skybox cubemap (for IBL reflections)
	Array setLayouts = {
		sceneUBO->layout.vk,
		models[0].model->layout,
		skybox.GetLayout()
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
		.SetFragVerShaders(sceneShader.shader, sceneShader.shader)
		.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetPolygonMode(VK_POLYGON_MODE_FILL)
		.SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.EnableMultisampling(VK_SAMPLE_COUNT_1_BIT)
		.DisableBlending()
		.EnableDepthTest(true, VK_COMPARE_OP_LESS)
		.SetColorAttachmentFormat(TextureFormat::BGRA8_SRGB) // TODO(Orgest): fine for now but needs to get the swapchain format
		.SetDepthAttachmentFormat(TextureFormat::D32_SFLOAT)
		// .EnableHotReload("Source/Game/Shaders/scene.slang", false)  // Enable hot reload for scene shader
		.Layout(scenePipeline.vkLayout);

	if (!scenePipeline.Create(device, layoutDesc, builder))
	{
		LOG(Error, "Failed to create scene pipeline");
		return false;
	}

	DrawLoadingSplash("Initializing Debug Renderer...");
	if (!debugRenderer.Initialize(device, &renderArena, sceneUBO, globalDescriptorAlloc, true, false))
	{
		LOG(Warning, "Debug renderer initialization failed - AABB debugging disabled");
	}

	// Initialize Scene Renderer - abstracts all model rendering logic
	{
		SceneRenderConfig renderConfig{
			.scenePipeline = &scenePipeline,
			.sceneUBO = sceneUBO,
			.skybox = &skybox,
			.debugRenderer = &debugRenderer,
			.models = &models,
		};
		sceneRenderer.Init(renderConfig);
	}

	ComputeStaticSceneStats();

	camera.position = {0.0f, 5.0f, -20.0f};  // Far back and elevated to see the grid
	camera.forward = glm::normalize(glm::vec3{0.0f, -0.2f, 1.0f});  // Looking slightly down toward center
	camera.up = {0, 1, 0};
	camera.UpdateDirectionVectors();

	activeCamera = &camera;
	camMode = CameraMode::FreeFly;
	editorUI->state.activeCamera  = activeCamera;


	lights.clear();

	// Create 4 point lights surrounding the sphere grid (grid is at z=-10, centered at y=5)
	constexpr float gridCenterZ = -10.0f;   // Grid depth position
	constexpr float lightHeight = 5.0f;     // Same height as sphere grid center
	constexpr float lightDistance = 12.0f;  // Distance from grid center
	constexpr float lightIntensity = 150.0f;
	constexpr float lightRange = 25.0f;

	// Front (magenta)
	{
		LightUBO L{};
		L.type = static_cast<u32>(LightType::Point);
		L.position = {0.0f, lightHeight, gridCenterZ + lightDistance};
		L.color = {1.0f, 0.2f, 1.0f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	// Back (cyan)
	{
		LightUBO L{};
		L.type = static_cast<u32>(LightType::Point);
		L.position = {0.0f, lightHeight, gridCenterZ - lightDistance};
		L.color = {0.2f, 1.0f, 1.0f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	// Right (pinkish)
	{
		LightUBO L{};
		L.type = static_cast<u32>(LightType::Point);
		L.position = {lightDistance, lightHeight, gridCenterZ};
		L.color = {1.0f, 0.4f, 0.8f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	// Left (blue)
	{
		LightUBO L{};
		L.type = static_cast<u32>(LightType::Point);
		L.position = {-lightDistance, lightHeight, gridCenterZ};
		L.color = {0.4f, 0.6f, 1.0f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	lightMeta.count = static_cast<u32>(lights.size());

	debugData.debugMode = DebugView::Material;
	Platform::ShowWindow(*windowContext);
	return true;
}

void Application::UpdateCamera()
{
	const float dt = windowContext->GetDeltaTime();
	const bool focused = windowContext->displayState.isFocused;
	const ImGuiIO& ioImgui = ImGui::GetIO();

	// Persistent cursor state
	static bool sLocked   = false;
	static bool sHadFocus = true;

	// Enable mouse look only if focused and ImGui not capturing
	input.mouseLookActive = focused && !ioImgui.WantCaptureMouse;

	// Handle focus gain/loss
	if (focused != sHadFocus)
	{
		if (!focused)
		{
			// Lost focus , unlock cursor, flush input
			if (sLocked)
			{
				Platform::LockCursor(*windowContext, false);
				sLocked = false;
			}

			Input::ResetInputOnFocusLoss();
			input.xrel = input.yrel = 0.0f;
			input.scrollX = input.scrollY = 0;
		}
		else
		{
			// Regained focus , clear residual deltas
			input.xrel = input.yrel = 0.0f;
			input.scrollX = input.scrollY = 0;

			if (camMode == CameraMode::FPS && !sLocked)
			{
				Platform::LockCursor(*windowContext, true);
				sLocked = true;
			}
		}
		sHadFocus = focused;
	}


	// Mode & feature toggles
	if (input.IsKeyDown(Keyboard::F1))
	{
		if (camMode == CameraMode::FreeFly)
		{
			// --- FreeFly , FPS ---
			fpsCamera.position = camera.position;
			fpsCamera.yaw      = camera.yaw;
			fpsCamera.pitch    = camera.pitch;
			fpsCamera.fov      = camera.fov;
			fpsCamera.UpdateDirectionVectors();
			fpsCamera.SyncBodyFromCameraStanding();

			camMode = CameraMode::FPS;
			activeCamera = &fpsCamera;

			if (focused && !sLocked)
			{
				Platform::LockCursor(*windowContext, true);
				sLocked = true;
			}
		}
		else
		{
			// --- FPS , FreeFly ---
			camera.position = fpsCamera.position;
			camera.yaw      = fpsCamera.yaw;
			camera.pitch    = fpsCamera.pitch;
			camera.fov      = fpsCamera.fov;
			camera.UpdateDirectionVectors();

			camMode = CameraMode::FreeFly;
			activeCamera = &camera;

			if (sLocked)
			{
				Platform::LockCursor(*windowContext, false);
				sLocked = false;
			}
		}
	}

	if (input.IsKeyDown(Keyboard::F2))
		debugRenderer.enabled = !debugRenderer.enabled;

	if (input.IsKeyDown(Keyboard::F3))
		showMenuBar = !showMenuBar;

	if (input.IsKeyDown(Keyboard::F4))
		showGPUInfo = !showGPUInfo;

	if (input.IsKeyDown(Keyboard::F8))
	{
		swapchain->presentMode =
			(swapchain->presentMode == PresentMode::VSyncOn)
			? PresentMode::VSyncOff
			: PresentMode::VSyncOn;

		swapchain->needsRecreation = true;

		LOG(Debug, "VSync: {}", swapchain->presentMode == PresentMode::VSyncOn ? "ON" : "OFF");
	}

	// Cursor lock control (FPS unlocks when Alt held)
	const bool wantLock = focused && camMode == CameraMode::FPS && !input.IsKeyHeld(Keyboard::Alt);
	if (wantLock != sLocked)
	{
		Platform::LockCursor(*windowContext, wantLock);
		sLocked = wantLock;
	}


	// Early-out if not focused
	if (!focused)
		return;

	// FPS mode
	if (camMode == CameraMode::FPS)
	{
		const bool allowMouseLook = input.mouseLookActive && !input.IsKeyHeld(Keyboard::Alt);
		fpsCamera.Update(dt, allowMouseLook);

		if (sLocked && allowMouseLook)
			Platform::CenterMouse(windowContext);

		input.xrel = input.yrel = 0.0f;
		return;
	}

	// FreeFly mode

	const bool isMouseLooking =
		input.mouseLookActive &&
		(input.mouseButtons[Mouse::Left].held ||
		 input.mouseButtons[Mouse::Right].held ||
		 input.mouseButtons[Mouse::Middle].held);

	if (isMouseLooking && focused && !windowContext->displayState.isResizing)
	{
		constexpr float MOUSE_SENS = 0.1f;
		camera.yaw   -= static_cast<float>(input.xrel * MOUSE_SENS);
		camera.pitch -= static_cast<float>(input.yrel * MOUSE_SENS);
		camera.pitch  = std::clamp(camera.pitch, -89.0f, 89.0f);
		camera.UpdateDirectionVectors();

		if (camMode == CameraMode::FreeFly)
			Platform::WrapCursorToOppositeEdge(windowContext);

		input.xrel = input.yrel = 0.0f;
	}

	// --- Alt + MMB Pan ---
	if (input.mouseButtons[Mouse::Middle].held && input.IsKeyHeld(Keyboard::Alt))
	{
		constexpr float PAN_SENS = 0.04f;
		camera.position += camera.right * static_cast<float>(-input.xrel * PAN_SENS);
		camera.position += camera.up    * static_cast<float>( input.yrel * PAN_SENS);
		input.xrel = input.yrel = 0.0f;
	}

	// --- WASDQE Movement ---
	glm::vec3 move{0.0f};
	if (input.IsKeyHeld(Keyboard::W)) move += camera.forward;
	if (input.IsKeyHeld(Keyboard::S)) move -= camera.forward;
	if (input.IsKeyHeld(Keyboard::A)) move -= camera.right;
	if (input.IsKeyHeld(Keyboard::D)) move += camera.right;
	if (input.IsKeyHeld(Keyboard::Q)) move -= camera.up;
	if (input.IsKeyHeld(Keyboard::E)) move += camera.up;

	if (glm::length2(move) > 1.0f)
		move = glm::normalize(move);

	camera.position += move * cameraSpeed * dt;

	// broken for now
	if (input.scrollY != 0 && !ioImgui.WantCaptureMouse)
	{
		constexpr float SPEED_STEP = 0.02f;
		cameraSpeed = std::clamp(cameraSpeed + input.scrollY * SPEED_STEP, 0.1f, 100.0f);
		cameraSpeedPopupTime = 1.5f;
	}

	input.scrollY = input.scrollX = 0;

	if (cameraSpeedPopupTime > 0.0f)
		cameraSpeedPopupTime -= dt;
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

	// Animate spinning lights around the grid
	static float lightTime = 0.0f;
	lightTime += windowContext->GetDeltaTime() * 0.1f; // Slow rotation (one full circle ~63 seconds)

	// Update 4 point lights to spin around the grid
	if (lights.size() >= 4)
	{
		constexpr float gridCenterZ = -10.0f;
		constexpr float lightHeight = 5.0f;
		constexpr float lightDistance = 12.0f;
		// Front light (rotates in XZ plane)
		float angle1 = lightTime;
		lights[0].position = glm::vec3(
			lightDistance * std::sin(angle1),
			lightHeight,
			gridCenterZ + (lightDistance * std::cos(angle1))
		);

		// Backlight (opposite side, 180 degrees offset)
		float angle2 = lightTime + glm::pi<float>();
		lights[1].position = glm::vec3(
			lightDistance * std::sin(angle2),
			lightHeight,
			gridCenterZ + (lightDistance * std::cos(angle2))
		);

		// Right light (90 degrees offset)
		float angle3 = lightTime + glm::half_pi<float>();
		lights[2].position = glm::vec3(
			lightDistance * std::sin(angle3),
			lightHeight,
			gridCenterZ + (lightDistance * std::cos(angle3))
		);

		// Left light (270 degrees offset)
		float angle4 = lightTime + glm::three_over_two_pi<float>();
		lights[3].position = glm::vec3(
			lightDistance * std::sin(angle4),
			lightHeight,
			gridCenterZ + (lightDistance * std::cos(angle4))
		);
	}

	// keep flashlight glued to camera
	for (auto& l : lights)
	{
		if (l.type == LightType::Spot) //// ?????
		{
			l.position = activeCamera->position;
			l.direction = glm::normalize(activeCamera->forward);
			break;
		}
	}

	lightMeta.count = static_cast<u32>(lights.size());

	const u32 frameIndex = frame.frameIndex;
	if (!sceneUBO)
	{
		LOG(Error, "Scene UBO is null at {}", frameIndex);
		return;
	}
	sceneUBO->UpdateBinding(frameIndex, 2, &debugData, sizeof(DebugUBO));
	sceneUBO->UpdateBinding(frameIndex, 3, &camUBO, sizeof(CameraUBO));
	sceneUBO->UpdateBinding(frameIndex, 4, lights.data(), sizeof(LightUBO) * lights.size());
	sceneUBO->UpdateBinding(frameIndex, 5, &lightMeta, sizeof(LightUBOCount));
	sceneUBO->UpdateBinding(frameIndex, 6, &sceneData, sizeof(SceneUBO));
}

void Application::RenderScene(const Renderer::FrameContext& frame)
{
	const auto* vkCmd = static_cast<Renderer::VulkanCommandBuffer*>(frame.commandContext);
	const VkCommandBuffer cmd = vkCmd->GetVkHandle();
	TracyVkZone(vkCmd->tracyCtx, cmd, "RenderScene");
	const u32 imageIndex = frame.imageIndex;
	const Extent2D extent = swapchain->GetExtent();

	auto& colorImage = swapchain->images[imageIndex];
	auto& depthImage = swapchain->depthImage;

	// Fetch GPU timing from PREVIOUS frame (that just finished via fence wait in BeginFrame)
	// Timestamps: 0=Scene Start, 1=Scene End, 2=Skybox End, 3=ImGui End
#ifdef ENABLE_GPU_TIMING
	if (frame.frameData->queryPool.FetchResults())
	{
		sceneStats.gpuDrawTime = frame.frameData->queryPool.DeltaMs(0, 2); // Total GPU time (all stages)
	}
#endif

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

	Renderer::VulkanRenderer::SetViewportAndScissor(cmd, extent);

	sceneStats.drawCallCount = 0;
	const auto cpuStart = std::chrono::high_resolution_clock::now();
#ifdef ENABLE_GPU_TIMING
	frame.frameData->queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0);
#endif

	// Render all scene models using abstracted renderer
	{
		TracyVkZone(frame.commandContext->tracyCtx, cmd, "RenderModels");
		sceneRenderer.RenderModels(cmd, frame.frameIndex, sceneStats);
	}

	// Draw skybox LAST with depth test enabled (will only draw where depth == 1.0)
	{
		TracyVkZone(frame.commandContext->tracyCtx, cmd, "Skybox");
		skybox.Render(cmd, *activeCamera, aspectRatio);
		sceneStats.drawCallCount++;
	}

#ifdef ENABLE_GPU_TIMING
	frame.frameData->queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 1);
#endif

	// Draw debug visualizations
	if (debugRenderer.enabled)
	{
		TracyVkZone(frame.commandContext->tracyCtx, cmd, "DrawAABBs");
		debugRenderer.Flush(cmd, frame.frameIndex);
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

	// Loop over all model components (the instances in the scene)
	for (const auto& comp : models)
	{
		if (comp.model == nullptr) continue;

		const auto* model = comp.model;

		if (model->vertexBuffer.buffer != VK_NULL_HANDLE)
		{
			sceneStats.totalVerts += static_cast<u32>(model->vertexBuffer.allocationInfo.size / sizeof(Vertex));
		}

		for (const auto& part : model->parts)
		{
			sceneStats.totalTris += part.indexCount / 3;
			++sceneStats.totalMeshCount;
		}
	}

	// Log scene complexity once at startup
	const Extent2D extent = swapchain->GetExtent();
	LOG(Debug, "Scene Stats - Triangles: {} | Verts: {} | MeshParts: {} | Resolution: {}x{}",
		sceneStats.totalTris, sceneStats.totalVerts, sceneStats.totalMeshCount,
		extent.width, extent.height);
}

void Application::RenderImGui(const Renderer::FrameContext& frame)
{
	auto* vkCmd = static_cast<Renderer::VulkanCommandBuffer*>(frame.commandContext);
	VkCommandBuffer cmd = vkCmd->GetVkHandle();
	TracyVkZone(vkCmd->tracyCtx, cmd, "ImGui");

	const u32 imageIndex = frame.imageIndex;
	const Extent2D extent = swapchain->GetExtent();
	auto& colorImage = swapchain->images[imageIndex];

	const Renderer::RenderPassDesc renderPassDesc
	{
		.renderPasses = SPAN_ONE(colorImage), // just the 1 color target
	};

	renderPass->Begin(cmd, extent, renderPassDesc, false);

	// if (editorUI->state.showDemoWindow) ImGui::ShowDemoWindow();

	if (showMenuBar)
	{
		if (editorUI->DrawMainMenuBar())
		{
			Cleanup();
			return;
		}
	}

	// Camera gizmo (move/rotate arrows, frustum, etc.)
	editorUI->DrawCameraGizmo(activeCamera);

	// Main overlay (GPU info, FPS, stats)
	if (showGPUInfo || debugRenderer.enabled)
	{
		editorUI->DrawMainOverlay();
	}

	// Light editor + 2D gizmos
	if (showLightMenu)
	{
		editorUI->DrawLightEditor();
	}

	// Camera speed popup
	editorUI->DrawCameraSpeedPopup(cameraSpeedPopupTime);


	// Camera property panel (FOV, near/far clip, speeds, etc.)
	editorUI->DrawCameraProperties(*activeCamera);

	// editorUI->DrawInputDebugPanel(input);

	EditorUI::EndFrame();
	EditorUI::Render(cmd);

#ifdef ENABLE_GPU_TIMING
	frame.frameData->queryPool.WriteTimestamp(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 3);
#endif


	// Compute GPU busy percentage
	const f32 frameMs = (lastFrameMs > 0.0f) ? lastFrameMs : (1000.0f / windowContext->fps);
	const float busy = (frameMs > 0.0f) ? (sceneStats.gpuDrawTime / frameMs * 100.0f) : 0.0f;
	sceneStats.gpuBusy = std::clamp(busy, 0.0f, 100.0f);

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
#if ENGINE_PLATFORM_SDL
			Input::ProcessEvents();
#else
			gameInput.Update(*windowContext);
#endif
		}

		Platform::StartFrame(*windowContext);

		if (renderer->ResizeIfNeeded()) { FrameMarkEnd("Frame"); continue; }

		// Check if swapchain needs recreation (e.g., VSync mode change)
		if (swapchain->needsRecreation) {
			swapchain->Recreate();
			swapchain->needsRecreation = false;
			FrameMarkEnd("Frame");
			continue;
		}

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

		Input::EndFrameInputUpdate();
		renderer->EndFrame(frame);
#if EDITORUI
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::RenderPlatformWindowsDefault();
		}
#endif
		FrameMarkEnd("Frame");
	}
}

void Application::Cleanup() const
{
	vkDeviceWaitIdle(device->device);

	gDeletionQueue.FlushLIFO();
	// if (device && device->device != VK_NULL_HANDLE)
	// {
	// 	vkDeviceWaitIdle(device->device);
	// }
	//
	// // 1) Pipelines first (they reference descriptor set layouts)
	// scenePipeline.Destroy();
	//
	// // 2) Cleanup managers (skybox, debug renderer)
	// skybox.Cleanup();
	// debugRenderer.Cleanup();
	//
	// // 3) Shader modules
	// if (sceneShader) { sceneShader->Destroy(); }
	//
	// // 5) Models (descriptor layouts + GPU buffers)
	// if (modelInst) modelInst->Destroy(device);
	// if (cubeMesh) cubeMesh->Destroy(device);
	// if (sphereMesh) sphereMesh->Destroy(device);
	//
	// // 6) Scene UBO (must be destroyed before global descriptor allocator)
	// if (sceneUBO) { sceneUBO->Destroy(); }
	//
	// // 7) Fallback textures and samplers
	// checkerboardImage.Destroy();
	// whiteImage.Destroy();
	// normalFallbackImage.Destroy();
	// if (checkerboardSampler) checkerboardSampler->Destroy();
	// if (normalFallbackSampler) normalFallbackImage.Destroy();
	//
	// // 8) Global descriptor allocator
	// if (globalDescriptorAlloc) globalDescriptorAlloc->DestroyPools();
	//
	// // 10) Renderer/UI/Swapchain
	// if (editorUI) editorUI->Destroy();
	// if (renderer) renderer->Destroy();
	// if (swapchain) swapchain->Destroy();
	//
	// // 11) Device/Instance last
	// if (device) device->Destroy();
	// if (instance) instance->Destroy();
}

void Application::CreatePBRSphereGrid()
{
	ZoneScopedN("CreatePBRSphereGrid");

	LoadedModel loadedModel;
	loadedModel.meshes.push_back(MeshGenerator::GenerateSphere());
	loadedModel.sourceType = MeshSourceType::Runtime;

	Material whiteMat;
	whiteMat.name = "SphereMaterial";
	whiteMat.roughness = 1.0f;
	whiteMat.metallic = 1.0f;
	loadedModel.materials.push_back(whiteMat);

	Renderer::TextureOrFallback fallback = {
		.fallbackImage = &whiteImage,
		.fallbackSampler = checkerboardSampler,
		.fallbackNormalImage = &normalFallbackImage,
		.fallbackNormalSampler = normalFallbackSampler
	};

	sphereMesh = renderArena.Emplace<Renderer::VulkanModel>(device, loadedModel, fallback);

	constexpr int rows = 7;
	constexpr int cols = 7;
	constexpr float spacingY = 2.5f;
	constexpr float spacingX = 2.5f;
	constexpr float gridOffsetY = (rows - 1) * spacingY * 0.5f;
	constexpr float gridOffsetX = (cols - 1) * spacingX * 0.5f;

	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < cols; ++col)
		{
			float x = (static_cast<float>(col) * spacingX) - gridOffsetX;
			float y = (static_cast<float>(row) * spacingY) - gridOffsetY + 5.0f;
			float z = -10.0f;

			Renderer::ModelComponent comp{
				.model = sphereMesh,
				.transform = glm::translate(glm::vec3(x, y, z)),
				.roughness = static_cast<float>(row) / static_cast<float>(rows - 1),
				.metallic = static_cast<float>(col) / static_cast<float>(cols - 1)
			};

			models.push_back(comp);
		}
	}
}

