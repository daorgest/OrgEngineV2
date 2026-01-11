//
// Created by Orgest on 7/13/2025.
//

#include "Application.h"

#include <algorithm>
#include <chrono>

#include "imgui.h"
#include "glm/gtc/constants.hpp"
#include "glm/gtx/norm.hpp"
#include "Input/InputSys.h"
#include "tracy/Tracy.hpp"
#include "MeshGenerator.h"
#include "MeshLoader.h"
#include "../Engine/ShaderConstants.h"
#include "glm/gtx/transform.hpp"
#include "Input/InputSysGameInput.h"

bool Application::Init()
{
	ZoneScopedN("Application::Init");

	{
		ZoneScopedN("Init Platform & Window");
		Platform::Init(&windowContext);
		// Platform::ShowWindow(*wc);
	}

	{
		ZoneScopedN("Init Vulkan Instance, Device, Swapchain, Editor UI, and Command Buffers");
		instance.Init();
		device.Init(&instance);
		swapchain.Init(&device, windowContext.handle);
		renderer.Init(&device, &swapchain);

		editorUI.state.wc            = &windowContext;
		editorUI.state.device        = &device;
		editorUI.state.swapchain     = &swapchain;
		editorUI.state.debugRenderer = &debugRenderer;
		editorUI.state.sceneStats    = &sceneStats;
		editorUI.state.lights        = &lights;
		editorUI.state.debugData     = &debugData;
		editorUI.state.cameraSpeed   = cameraSpeed;

		editorUI.Init(&instance, &device, &swapchain);
	}
	// DrawLoadingSplash("Loading...");

	{
		ZoneScopedN("Init Global Descriptor Allocator");
		Array<Renderer::PoolSizes, 5> sizes = {
			{DescriptorType::CombinedImageSampler, 4000.f},
			{DescriptorType::UniformBuffer, 10.f},
			{DescriptorType::StorageBuffer, 10.f},
			{DescriptorType::SampledImage, 10.f},
			{DescriptorType::Sampler, 10.f},
		 };
		globalDescriptorAlloc.Init(&device, 10, sizes);
	}

	{
		ZoneScopedN("Init Scene UBO");
		DescriptorSetLayoutDesc sceneDesc = {
			.setIndex = 0,
			.bindings = Constants::Scene
		};
		sceneUBO = std::make_unique<Renderer::VulkanShaderBuffer>(&device, &globalDescriptorAlloc, sceneDesc);
		sceneUBO->AllocateDescriptorSets();
	}

	// Texture Defaults

	texDefaults.Init(&device);

	// // Main model
	// {
	// 	ZoneScopedN("LoadModel From Source!");
	// 	DrawLoadingSplash("Loading OBJ Model...");
	// 	LOG(Debug, "Loading main model: Sponza/sponza.obj");
	//
	// 	auto result = Assets::MeshLoader::LoadModelFromSource(MeshSourceType::OBJ, "Sponza/sponza.obj");
	// 	modelInst = std::make_unique<Renderer::VulkanModel>(&device, *result, globalDescriptorAlloc, texDefaults);
	// 	Renderer::ModelComponent comp = {
	//
	// 		.model = modelInst.get(),
	// 		.transform = glm::scale(glm::vec3(0.02f))
	// 	};
	// 	models.push_back(comp);
	// }

	// DrawLoadingSplash("Building Skybox...");

	if (!skybox.Initialize(&device, &renderArena))
	{
		LOG(Warning, "Skybox initialization failed - Skybox will not be rendered");
	}

	// DrawLoadingSplash("Creating PBR Sphere Grid...");

	CreatePBRSphereGrid();

	DrawLoadingSplash("Compiling Shaders...");
	auto codeResult = Renderer::VulkanShader::ReadShaderFile("Shaders/scene.spv");
	if (!codeResult)
	{
		LOG(Error, "Failed to load shader: Shaders/scene.spv ({})", static_cast<i32>(codeResult.error()));
		return false;
	}

	sceneShader.Init(&device, codeResult.value());

	// DrawLoadingSplash("Initializing Debug Renderer...");
	if (!debugRenderer.Initialize(&device, &renderArena, sceneUBO.get(), &globalDescriptorAlloc, true, false))
	{
		LOG(Warning, "Debug renderer initialization failed - AABB debugging disabled");
	}

	// Initialize Scene Renderer - abstracts all model rendering logic

	SceneRenderConfig renderConfig = {
		.device = &device,
		.vertexShader = &sceneShader,
		.fragmentShader = &sceneShader,
		.descriptorAllocator = &globalDescriptorAlloc,
		.sceneUBO = sceneUBO.get(),
		.skybox = &skybox,
		.debugRenderer = &debugRenderer,
		.models = &models,
	};
	sceneRenderer.Init(renderConfig);

	ComputeStaticSceneStats();

    for (i32 i = 0; i < MAX_SCENE_CAMERAS; i++)
    {
        auto& camComp = sceneCameras[i];

        camComp.position = {0.0f, 5.0f, -20.0f};

        // Base Camera Defaults
        camComp.base.fov = 70.0f;
        camComp.base.nearPlane = 0.01f;
        camComp.base.farPlane = 10000.0f;

        // Controller Defaults
        camComp.controller.fovBase = 70.0f;
        camComp.controller.eyeHeight = 1.6f;

        camComp.base.UpdateVecAndMat(camComp.position, aspectRatio);
    }

    camMode = CameraMode::FreeFly;
    activeIdx = 0;
    editorUI.state.cameraComponents = std::span(sceneCameras.data(), MAX_SCENE_CAMERAS);
    editorUI.state.activeCameraIdx = activeIdx;

	// Create 4 point lights surrounding the sphere grid (grid is at z=-10, centered at y=5)
	constexpr f32 gridCenterZ = -10.0f;   // Grid depth position
	constexpr f32 lightHeight = 5.0f;     // Same height as sphere grid center
	constexpr f32 lightDistance = 12.0f;  // Distance from grid center
	constexpr f32 lightIntensity = 150.0f;
	constexpr f32 lightRange = 25.0f;

	// Front (magenta)
	{
		LightUBO L{};
		L.type = LightType::Point;
		L.position = {0.0f, lightHeight, gridCenterZ + lightDistance};
		L.color = {1.0f, 0.2f, 1.0f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	// Back (cyan)
	{
		LightUBO L{};
		L.type = LightType::Point;
		L.position = {0.0f, lightHeight, gridCenterZ - lightDistance};
		L.color = {0.2f, 1.0f, 1.0f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	// Right (pinkish)
	{
		LightUBO L{};
		L.type = LightType::Point;
		L.position = {lightDistance, lightHeight, gridCenterZ};
		L.color = {1.0f, 0.4f, 0.8f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	// Left (blue)
	{
		LightUBO L{};
		L.type = LightType::Point;
		L.position = {-lightDistance, lightHeight, gridCenterZ};
		L.color = {0.4f, 0.6f, 1.0f};
		L.intensity = lightIntensity;
		L.range = lightRange;
		lights.push_back(L);
	}

	lightMeta.count = static_cast<u32>(lights.size());

	debugData.debugMode = DebugView::Material;
	Platform::ShowWindow(windowContext);
	return true;
}


void Application::Run()
{
	while (Platform::ProcessMessages(&windowContext))
	{
		FrameMarkStart("Frame");
		ZoneScopedN("Frame");

		{
			ZoneScopedN("GameInput Update");
#if ENGINE_PLATFORM_WIN32
			gameInput.Update(windowContext);
#endif
		}

		Platform::StartFrame(windowContext);

		if (!swapchain.ResizeIfNeeded())
		{
			FrameMarkEnd("Frame");
			continue;
		}

		u32 frameIndex = 0;
		u32 imageIndex = 0;
	    if (!renderer.BeginFrame(frameIndex, imageIndex)) continue;

	    {
		    ZoneScopedN("Update Logic");
		    UpdateCamera();
		    UpdateSceneUBO();
	    }

	    {
		    ZoneScopedN("Render Logic");
		    RenderScene(imageIndex);

		    EditorUI::BeginFrame();
		    RenderImGui(imageIndex);
	    }

		Input::EndFrameInputUpdate();
		renderer.EndFrame(frameIndex, imageIndex);
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
	vkDeviceWaitIdle(device.device);
}


void Application::UpdateCamera()
{
    const f32 dt = windowContext.GetDeltaTime();
    if (!windowContext.displayState.isFocused) return;

    const ImGuiIO& io = ImGui::GetIO();
    input.mouseLookActive = !io.WantCaptureMouse;

    CameraComponent& activeCam = sceneCameras[activeIdx];

    if (input.usingKeyboard)
    {
        // F1: Toggle between FPS and FreeFly mode
        if (input.IsKeyDown(Keyboard::F1))
        {
            const bool toFPS = (camMode == CameraMode::FreeFly);
            camMode = toFPS ? CameraMode::FPS : CameraMode::FreeFly;

            // When switching to FPS, we "drop" the foot position to the ground
            // When switching to FreeFly, we "lift" the position back to head-level
            const float offset = activeCam.controller.eyeHeight;
            activeCam.position.y += toFPS ? -offset : offset;

            if (camMode == CameraMode::FPS) {
                activeCam.controller.velocity = glm::vec3(0.0f);
                activeCam.controller.grounded = false; // Force re-check of collision
            }
        }

        // F2-F6: System toggles
        if (input.IsKeyDown(Keyboard::F2)) debugRenderer.enabled = !debugRenderer.enabled;
        if (input.IsKeyDown(Keyboard::F3)) showMenuBar = !showMenuBar;
        if (input.IsKeyDown(Keyboard::F4)) showGPUInfo = !showGPUInfo;
        if (input.IsKeyDown(Keyboard::F5)) {
            swapchain.presentMode = (swapchain.presentMode == PresentMode::VSyncOn) ? PresentMode::VSyncOff : PresentMode::VSyncOn;
            swapchain.needsRecreation = true;
        }
        if (input.IsKeyDown(Keyboard::F6)) editorUI.state.noUI = !editorUI.state.noUI;
        if (input.IsKeyDown(Keyboard::F7)) freezeFrustum = !freezeFrustum;
        if (input.IsKeyDown(Keyboard::F8)) activeIdx = (activeIdx + 1) % MAX_SCENE_CAMERAS;

    }

    const bool altHeld = input.IsKeyHeld(Keyboard::Alt);
    const bool shouldLock = camMode == CameraMode::FPS && !altHeld;

    Platform::SetCursorLocked(&windowContext, shouldLock);
    Platform::SetCursorVisible(!shouldLock);

    glm::vec3 renderPos = activeCam.position;
    if (camMode == CameraMode::FPS)
    {
        // 1. Update feet and timers
        activeCam.controller.Update(activeCam, dt);

        // 2. Apply Bobbing to the eye offset
        const float s = std::sin(activeCam.controller.headTimer * glm::two_pi<float>());
        const float c = std::cos(activeCam.controller.headTimer * glm::two_pi<float>());

        glm::vec3 bobOffset = activeCam.base.right * (s * activeCam.controller.tune.bobHorizAmp);
        bobOffset.y = std::abs(c * activeCam.controller.tune.bobVertAmp);

        // 3. Render Position = Feet + Height + Bob
        renderPos = activeCam.position + glm::vec3(0, activeCam.controller.eyeHeight, 0) + bobOffset;

        if (shouldLock) Platform::CenterMouse(&windowContext);
    }
    else
    {
        ApplyFreeFlyMovement(activeIdx, dt);
        renderPos = activeCam.position;
    }


    if (input.scrollY != 0 && !io.WantCaptureMouse)
    {
        cameraSpeed = std::clamp(cameraSpeed * ((input.scrollY > 0) ? 1.1f : 0.9f), 0.1f, 500.0f);
        cameraSpeedPopupTime = 1.5f;
    }
    cameraSpeedPopupTime = std::max(0.0f, cameraSpeedPopupTime - dt);

    if (!freezeFrustum) frustumIdx = activeIdx;

    activeCam.base.UpdateVecAndMat(renderPos, aspectRatio);

    editorUI.state.activeCameraIdx = activeIdx;
    editorUI.state.selectedCameraIdx = selectedCameraIdx;
}

void Application::ApplyFreeFlyMovement(const u32 idx, const f32 dt)
{
    CameraComponent& camComp = sceneCameras[idx];
    Camera& cam = camComp.base;
    glm::vec3& worldPos = camComp.position;

    const bool altHeld = input.IsKeyHeld(Keyboard::Alt);
    const bool allowLook = input.mouseLookActive && !altHeld;

    // 1. Rotation (Mouse)
    if (allowLook && (input.mouseButtons[Mouse::Right].held || input.mouseButtons[Mouse::Middle].held))
    {
        if (altHeld && input.mouseButtons[Mouse::Middle].held) {
            // Pan world position relative to camera vectors
            worldPos += (cam.right * (float)-input.xrel + cam.up * (float)input.yrel) * 0.04f;
        } else {
            cam.yaw -= (float)input.xrel * 0.1f;
            cam.pitch = std::clamp(cam.pitch - (float)input.yrel * 0.1f, -89.0f, 89.0f);
            Platform::WrapCursorToOppositeEdge(&windowContext);
        }
    }

    // 2. Translation (Keyboard)
    glm::vec3 move{0.0f};
    if (input.IsKeyHeld(Keyboard::W)) move += cam.forward;
    if (input.IsKeyHeld(Keyboard::S)) move -= cam.forward;
    if (input.IsKeyHeld(Keyboard::A)) move -= cam.right;
    if (input.IsKeyHeld(Keyboard::D)) move += cam.right;
    if (input.IsKeyHeld(Keyboard::Q)) move -= cam.up;
    if (input.IsKeyHeld(Keyboard::E)) move += cam.up;

    if (glm::length2(move) > 1e-6f) {
        worldPos += glm::normalize(move) * cameraSpeed * dt;
    }

}

void Application::UpdateSceneUBO()
{
    ZoneScopedN("UpdateSceneUBO");
    const u32 frameIndex = renderer.GetFrameIndex();
    const CameraComponent& activeCam = sceneCameras[activeIdx];

    glm::vec3 finalEyePos = activeCam.position;
    if (camMode == CameraMode::FPS)
    {
        finalEyePos.y += activeCam.controller.eyeHeight;
    }

    aspectRatio = static_cast<f32>(swapchain.width) / static_cast<f32>(swapchain.height);
    sceneData.view = activeCam.base.view;
    sceneData.proj = activeCam.base.projection;
    camUBO.position = finalEyePos;
    camUBO.nearPlane = activeCam.base.nearPlane;
    camUBO.farPlane = activeCam.base.farPlane;

    // Frustum Visualizers
    for (u32 i = 0; i < MAX_SCENE_CAMERAS; i++)
    {
        if (i == activeIdx) continue;
        if (i == frustumIdx || i == selectedCameraIdx)
        {
            QueueFrustumVisualizer(i, (i == frustumIdx) ? glm::vec4(0, 1, 1, 1) : glm::vec4(1, 1, 0, 1));
        }
    }

    lightMeta.count = static_cast<u32>(lights.size());

    TracyPlot("Draw Calls", static_cast<i64>(sceneStats.drawCallCount));
    TracyPlot("Triangle Count", static_cast<i64>(sceneStats.totalTris));
    TracyPlot("GPU Draw Time (ms)", sceneStats.gpuDrawTime);

    if (sceneUBO)
    {
        sceneUBO->UpdateBinding(frameIndex, 2, &debugData, sizeof(DebugUBO));
        sceneUBO->UpdateBinding(frameIndex, 3, &camUBO, sizeof(CameraUBO));
        sceneUBO->UpdateBinding(frameIndex, 4, lights.data(), sizeof(LightUBO) * lights.size());
        sceneUBO->UpdateBinding(frameIndex, 5, &lightMeta, sizeof(LightUBOCount));
        sceneUBO->UpdateBinding(frameIndex, 6, &sceneData, sizeof(SceneUBO));
    }
}

void Application::QueueFrustumVisualizer(u32 camIdx, const glm::vec4& color)
{
    if (!debugRenderer.enabled) return;

    const Camera& cam = sceneCameras[camIdx].base;
    const glm::mat4 invVP = glm::inverse(cam.projection * cam.view);

    static constexpr AABB ndcVolume = []() {
        AABB box;
        box.center  = glm::vec3(0.0f, 0.0f, 0.5f);
        box.extents = glm::vec3(1.0f, 1.0f, 0.5f);
        return box;
    }();

    debugRenderer.SetColor(color);
    debugRenderer.QueueBox(invVP, ndcVolume);
}

void Application::RenderScene(u32 imageIndex)
{
	auto* frame = static_cast<Renderer::VulkanFrameData*>(renderer.GetCurrentFrameData());
	auto& cmd = frame->commandBuffer;
	const Extent2D extent = swapchain.GetExtent();
	const u32 frameIndex = renderer.GetFrameIndex();

	Renderer::GPUTexture* colorImage = swapchain.GetImage(imageIndex);
	Renderer::GPUTexture* depthImage = &swapchain.depthTexture;

	frame->queryPool.Reset(&cmd);

#ifdef ENABLE_GPU_TIMING
	if (frame.frameData->queryPool.FetchResults())
	{
		sceneStats.gpuDrawTime = frame.frameData->queryPool.DeltaMs(0, 1); // Total GPU time (all stages)
	}
#endif

	// Transition attachments
	{
		cmd.BeginDebugLabel("Scene/Transitions", 0.0f, 0.7f, 1.0f);
		cmd.TransitionLayout(colorImage, TextureLayout::ColorWrite);
		cmd.TransitionLayout(depthImage, TextureLayout::DepthWrite);
		cmd.EndDebugLabel();
	}

	auto colorAttach = Renderer::RenderAttachment::Color(colorImage, LoadOP::Clear, {0.1f, 0.1f, 0.1f, 1.0f});
	auto depthAttach = Renderer::RenderAttachment::Depth(depthImage, LoadOP::Clear, 0.0);

	Renderer::RenderingInfo renderInfo = {
		.extent = extent,
		.colorAttachments = SPAN_ONE(colorAttach),
		.depthAttachment = &depthAttach
	};
	cmd.BeginRendering(renderInfo);

	cmd.SetViewport({0.0f, 0.0f, (f32)extent.width, (f32)extent.height, 0.0f, 1.0f});
	cmd.SetScissor(0, 0, extent.width, extent.height);

	const auto cpuStart = std::chrono::high_resolution_clock::now();
#ifdef ENABLE_GPU_TIMING
	frame.frameData->queryPool.WriteTimestamp(rawCmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0);
#endif

    CameraComponent& activeCam = sceneCameras[activeIdx];

    sceneRenderer.PrepareFrame(&windowContext, &activeCam.base, freezeFrustum);
    sceneRenderer.RenderModels(frame->GetCommandBuffer(), frameIndex, sceneStats);

    skybox.Render(&cmd, activeCam.base, aspectRatio);

	if (debugRenderer.enabled)
	{
		debugRenderer.Flush(&cmd, frameIndex);
	}

#ifdef ENABLE_GPU_TIMING
	frame.frameData->queryPool.WriteTimestamp(rawCmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 1);
#endif

	const auto cpuEnd = std::chrono::high_resolution_clock::now();
	sceneStats.cpuDrawTime = static_cast<f32>(std::chrono::duration<double, std::milli>(cpuEnd - cpuStart).count());

	cmd.EndRendering();
}

void Application::RenderImGui(u32 imageIndex)
{
	auto* frame = static_cast<Renderer::VulkanFrameData*>(renderer.GetCurrentFrameData());
	auto& cmd = frame->commandBuffer;
	VkCommandBuffer vkCmd = cmd.GetVkHandle();

	TracyVkZone(cmd.tracyCtx, vkCmd, "ImGui");

	const Extent2D extent = swapchain.GetExtent();
	Renderer::GPUTexture* colorImage = swapchain.GetImage(imageIndex);

	Renderer::RenderAttachment colorAttach = Renderer::RenderAttachment::Color(colorImage, LoadOP::Load);

	const Renderer::RenderingInfo renderInfo = {
		.extent = extent,
		.colorAttachments = SPAN_ONE(colorAttach),
	};

	cmd.BeginRendering(renderInfo);

    CameraComponent& activeCam = sceneCameras[activeIdx];

	// if (editorUI->state.showDemoWindow) ImGui::ShowDemoWindow();

	if (showMenuBar)
	{
		if (editorUI.DrawMainMenuBar())
		{
			Cleanup();
			return;
		}
	}

	// Camera gizmo (move/rotate arrows, frustum, etc.)
	editorUI.DrawCameraGizmo(activeCam);

	// Main overlay (GPU info, FPS, stats)
	if (showGPUInfo || debugRenderer.enabled)
	{
		editorUI.DrawMainOverlay();
	}

	// Light editor + 2D gizmos
	if (showEditorTools)
	{
	    editorUI.DrawEditorTools();
	}

    editorUI.UpdateLights(windowContext.GetDeltaTime());
	// Camera speed popup
	editorUI.DrawCameraSpeedPopup(cameraSpeedPopupTime);

	EditorUI::EndFrame();
	EditorUI::Render(frame->GetCommandBuffer());

	cmd.EndRendering();
	cmd.TransitionLayout(colorImage, TextureLayout::Present);

	// Compute GPU busy percentage
	const f32 frameMs = (lastFrameMs > 0.0f) ? lastFrameMs : (1000.0f / windowContext.fps);
	const f32 busy = (frameMs > 0.0f) ? (sceneStats.gpuDrawTime / frameMs * 100.0f) : 0.0f;
	sceneStats.gpuBusy = std::clamp(busy, 0.0f, 100.0f);
}

void Application::DrawLoadingSplash(const char* text)
{
	// Process a frame just like Run() does, but only once.
	if (!swapchain.ResizeIfNeeded()) return;

	u32 frameIndex = 0;
	u32 imageIndex = 0;
	if (!renderer.BeginFrame(frameIndex, imageIndex))
		return;

	auto* frameData = static_cast<Renderer::VulkanFrameData*>(renderer.GetCurrentFrameData());
	auto* cmd = static_cast<Renderer::VulkanCommandBuffer*>(frameData->GetCommandBuffer());

	Renderer::GPUTexture* colorImage = swapchain.GetImage(imageIndex);
	const Extent2D extent = swapchain.GetExtent();

	cmd->TransitionLayout(colorImage, TextureLayout::ColorWrite);
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
			ImVec2 win = ImGui::GetWindowSize();
			ImVec2 sz = ImGui::CalcTextSize(text);
			ImGui::SetCursorPos({(win.x - sz.x) * 0.5f, (win.y - sz.y) * 0.5f});
			ImGui::TextUnformatted(text);
		}
		ImGui::End();
	}

	auto colorAttach = Renderer::RenderAttachment::Color(colorImage, LoadOP::Clear, {0.0f, 0.0f, 0.0f, 1.0f});

	Renderer::RenderingInfo renderInfo = {
		.extent = extent,
		.colorAttachments = SPAN_ONE(colorAttach),
	 };

	cmd->BeginRendering(renderInfo);

	editorUI.EndFrame();

	editorUI.Render(cmd);

	cmd->EndRendering();

	cmd->TransitionLayout(colorImage, TextureLayout::Present);

	renderer.EndFrame(frameIndex, imageIndex);
}

void Application::CreatePBRSphereGrid()
{
	ZoneScopedN("CreatePBRSphereGrid3D");

	LoadedModel loadedModel;
	loadedModel.sourceType = MeshSourceType::Runtime;

	Mesh sphereSource = MeshGenerator::GenerateSphere();
	sphereSource.name = "PBR_Sphere_3D";
	loadedModel.meshes.push_back(std::move(sphereSource));

	Material pbrMat;
	pbrMat.name = "SpherePBRBase";
	pbrMat.baseColor = glm::vec3(1.0f);
	pbrMat.roughness = 1.0f;
	pbrMat.metallic = 1.0f;
	loadedModel.materials.push_back(pbrMat);

	sphereMesh = std::make_unique<Renderer::VulkanModel>(&device, loadedModel, globalDescriptorAlloc,  texDefaults);

	// 3D Grid Dimensions
	constexpr i32 dimX = 20; // Metallic variation
	constexpr i32 dimY = 20; // Roughness variation
	constexpr i32 dimZ = 20; // Depth stacking
	constexpr f32 spacing = 3.0f;

	constexpr f32 offsetX = (dimX - 1) * spacing * 0.5f;
	constexpr f32 offsetY = (dimY - 1) * spacing * 0.5f;
	constexpr f32 offsetZ = (dimZ - 1) * spacing * 0.5f;

	for (i32 z = 0; z < dimZ; ++z)
	{
		for (i32 y = 0; y < dimY; ++y)
		{
			for (i32 x = 0; x < dimX; ++x)
			{
				f32 posX = (x * spacing) - offsetX;
				f32 posY = (y * spacing) - offsetY + 10.0f;
				f32 posZ = (z * spacing) - offsetZ - 20.0f; // Push away from camera

				models.push_back(Renderer::ModelComponent{
					.model = sphereMesh.get(),
					.transform = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ)),
					.path = Renderer::RenderPath::Standard,
					// Map Roughness to Y-axis, Metallic to X-axis
					.roughness = glm::clamp(static_cast<f32>(y) / (dimY - 1), 0.05f, 1.0f),
					.metallic = static_cast<f32>(x) / (dimX - 1)
				});
			}
		}
	}
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
	const Extent2D extent = swapchain.GetExtent();
	LOG(Debug, "Scene Stats - Triangles: {} | Verts: {} | MeshParts: {} | Resolution: {}x{}",
	    sceneStats.totalTris, sceneStats.totalVerts, sceneStats.totalMeshCount,
	    extent.width, extent.height);
}
