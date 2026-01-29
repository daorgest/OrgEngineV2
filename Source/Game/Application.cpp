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
		CHECK_RESULT(swapchain.Init(&device, windowContext.handle));
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
	// 	auto result = Assets::MeshLoader::LoadModelFromSource(MeshSourceType::OBJ, "Assets/Sponza/sponza.obj");
	// 	modelInst = std::make_unique<Renderer::VulkanModel>(&device, *result, globalDescriptorAlloc, texDefaults);
	// 	Renderer::ModelComponent comp = {
	//
	// 		.model = modelInst.get(),
	// 		.transform = glm::scale(glm::vec3(0.01f)),
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

	// DrawLoadingSplash("Compiling Shaders...");
	auto codeResult = Renderer::VulkanShader::ReadShaderFile("Shaders/scene.spv");
	if (!codeResult)
	{
		LOG(Error, "Failed to load shader: Shaders/scene.spv ({})", static_cast<i32>(codeResult.error()));
		return false;
	}
    CHECK_RESULT(sceneShader.Init(&device, codeResult.value()));

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
    activeCamIdx = 0;
    editorUI.state.cameraComponents = std::span(sceneCameras.data(), MAX_SCENE_CAMERAS);
    editorUI.state.activeCameraIdx = activeCamIdx;

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

    InitDefaultBindings();

	debugData.debugMode = DebugView::Material;
	Platform::ShowWindow(windowContext);
	return true;
}

void Application::InitDefaultBindings()
{
    // --- Rebindable Movement (Keyboard + Gamepad) ---
    input.BindAction(Action::MoveForward,  Keyboard::W);
    input.BindAction(Action::MoveBackward, Keyboard::S);
    input.BindAction(Action::MoveLeft,     Keyboard::A);
    input.BindAction(Action::MoveRight,    Keyboard::D);
    input.BindAction(Action::MoveUp,       Keyboard::E);
    input.BindAction(Action::MoveDown,     Keyboard::Q);

    // Additive Gamepad bindings for movement
    input.BindAction(Action::MoveForward,  Gamepad::Button::DpadUp);
    input.BindAction(Action::MoveBackward, Gamepad::Button::DpadDown);
    input.BindAction(Action::MoveLeft,     Gamepad::Button::DpadLeft);
    input.BindAction(Action::MoveRight,    Gamepad::Button::DpadRight);

    // --- Strict Keyboard System Keys (F1–F9) ---
    input.BindAction(Action::ToggleFPS,      Keyboard::F1);
    input.BindAction(Action::ToggleDebug,    Keyboard::F2);
    input.BindAction(Action::ToggleMenuBar,  Keyboard::F3);
    input.BindAction(Action::ToggleGPUInfo,  Keyboard::F4);
    input.BindAction(Action::ToggleVSync,    Keyboard::F5);
    input.BindAction(Action::ToggleUI,       Keyboard::F6);
    input.BindAction(Action::ToggleFrustum,  Keyboard::F7);
    input.BindAction(Action::CycleCamera,    Keyboard::F8);
    input.BindAction(Action::CycleDebugView, Keyboard::F9);

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

		if (!swapchain.ResizeIfNeeded() || windowContext.displayState.isMinimized)
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
		FrameMarkEnd("Frame");
	}
}

void Application::Cleanup()
{
	device.WaitIdle();
}


void Application::UpdateCamera()
{
    if (!windowContext.displayState.isFocused) return;

    activeCamIdx = editorUI.state.activeCameraIdx;
    const f32 dt = windowContext.GetDeltaTime();

    CameraComponent& activeCam = sceneCameras[activeCamIdx];

    // F1: Toggle between FPS and FreeFly mode
    if (input.IsActionDown(Action::ToggleFPS))
    {
        const bool toFPS = (camMode == CameraMode::FreeFly);
        camMode = toFPS ? CameraMode::FPS : CameraMode::FreeFly;

        // When switching to FPS, we "drop" the foot position to the ground
        // When switching to FreeFly, we "lift" the position back to head-level
        const f32 offset = activeCam.controller.eyeHeight;
        activeCam.position.y += toFPS ? -offset : offset;

        if (camMode == CameraMode::FPS)
        {
            activeCam.controller.velocity = glm::vec3(0.0f);
            activeCam.controller.grounded = false; // Force re-check of collision
        }
    }

    // F2-F6: System toggles
    if (input.IsActionDown(Action::ToggleDebug)) debugRenderer.enabled = !debugRenderer.enabled;
    if (input.IsActionDown(Action::ToggleMenuBar)) editorUI.state.showMenuBar = !editorUI.state.showMenuBar;
    if (input.IsActionDown(Action::ToggleGPUInfo)) editorUI.state.showGPUInfo = !editorUI.state.showGPUInfo;
    if (input.IsActionDown(Action::ToggleVSync))
    {
        swapchain.presentMode = (swapchain.presentMode == PresentMode::VSyncOn)
                                    ? PresentMode::VSyncOff
                                    : PresentMode::VSyncOn;
        swapchain.needsRecreation = true;
    }
    if (input.IsActionDown(Action::ToggleUI)) editorUI.state.noUI = !editorUI.state.noUI;
    if (input.IsActionDown(Action::ToggleFrustum)) freezeFrustum = !freezeFrustum;
    if (input.IsActionDown(Action::CycleCamera))
    {
        activeCamIdx = (activeCamIdx + 1) % MAX_SCENE_CAMERAS;
        selectedCameraIdx = activeCamIdx;
        editorUI.state.selectedCameraIdx = selectedCameraIdx;
        editorUI.state.activeCameraIdx = activeCamIdx;
    }
    if (input.IsActionDown(Action::CycleDebugView))
    {
        // 1. Calculate the number of elements in your array
        constexpr i32 viewCount = std::size(kDebugViews);

        // 2. Find current index
        i32 currentIdx = 0;
        for (i32 i = 0; i < viewCount; i++)
        {
            if (kDebugViews[i].value == debugData.debugMode)
            {
                currentIdx = i;
                break;
            }
        }

        // 3. Modulo math to cycle
        i32 nextIdx = (currentIdx + 1) % viewCount;

        // 4. Update the actual engine state
        debugData.debugMode = kDebugViews[nextIdx].value;
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
        const f32 s = std::sin(activeCam.controller.headTimer * glm::two_pi<f32>());
        const f32 c = std::cos(activeCam.controller.headTimer * glm::two_pi<f32>());

        glm::vec3 bobOffset = activeCam.base.right * (s * activeCam.controller.tune.bobHorizAmp);
        bobOffset.y = std::abs(c * activeCam.controller.tune.bobVertAmp);

        // 3. Render Position = Feet + Height + Bob
        renderPos = activeCam.position + glm::vec3(0, activeCam.controller.eyeHeight, 0) + bobOffset;

        if (shouldLock) Platform::CenterMouse(&windowContext);
    }
    else
    {
        ApplyFreeFlyMovement(activeCamIdx, dt);
        renderPos = activeCam.position;
    }

    if (input.scrollY != 0)
    {
        cameraSpeed = std::clamp(cameraSpeed * ((input.scrollY > 0) ? 1.1f : 0.9f), 0.1f, 500.0f);
        editorUI.state.cameraSpeed = cameraSpeed;
        cameraSpeedPopupTime = 1.5f;
    }
    cameraSpeedPopupTime = std::max(0.0f, cameraSpeedPopupTime - dt);

    activeCam.base.UpdateVecAndMat(renderPos, aspectRatio);

    if (!freezeFrustum) frustumIdx = activeCamIdx;
    selectedCameraIdx = editorUI.state.selectedCameraIdx;
}

void Application::ApplyFreeFlyMovement(const u32 idx, const f32 dt)
{
    CameraComponent& camComp = sceneCameras[idx];
    Camera& cam = camComp.base;
    glm::vec3& worldPos = camComp.position;

    f32 deltaYaw = 0.0f;
    f32 deltaPitch = 0.0f;

    if (input.IsKeyHeld(Keyboard::Alt) && input.IsMouseHeld(Mouse::Middle))
    {
        worldPos += (cam.right * static_cast<f32>(-input.xrel) + cam.up * static_cast<f32>(input.yrel)) * 0.02f;
        input.scrollY = 0; // no cam speed happening here
    }

    if ((input.IsMouseHeld(Mouse::Right)))
    {
        deltaYaw   -= static_cast<f32>(input.xrel) * 0.1f;
        deltaPitch -= static_cast<f32>(input.yrel) * 0.1f;
        Platform::WrapCursorToOppositeEdge(&windowContext);
    }

    // Controller movement
    if (input.controllers[0].connected)
    {
        deltaYaw   -= input.GetRightStickX() * 150.0f * dt;
        deltaPitch += input.GetRightStickY() * 150.0f * dt;
    }

    cam.yaw += deltaYaw;
    cam.pitch = std::clamp(cam.pitch + deltaPitch, -89.0f, 89.0f);

    glm::vec3 move{0.0f};
    if (input.IsActionHeld(Action::MoveForward))  move += cam.forward;
    if (input.IsActionHeld(Action::MoveBackward)) move -= cam.forward;
    if (input.IsActionHeld(Action::MoveLeft))     move -= cam.right;
    if (input.IsActionHeld(Action::MoveRight))    move += cam.right;
    if (input.IsActionHeld(Action::MoveUp))       move += cam.up;
    if (input. IsActionHeld(Action::MoveDown))     move -= cam.up;

    if (input.controllers[0].connected)
    {
        move += cam.forward * input.GetLeftStickY();
        move += cam.right   * input.GetLeftStickX();
    }

    if (glm::length2(move) > 1e-6f) {
        worldPos += glm::normalize(move) * cameraSpeed * dt;
    }

}

void Application::UpdateSceneUBO()
{
    ZoneScopedN("UpdateSceneUBO");
    const u32 frameIndex = renderer.GetFrameIndex();
    const CameraComponent& activeCam = sceneCameras[activeCamIdx];

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
    // if (camIdx == activeIdx && !freezeFrustum) return;

    const Camera& cam = sceneCameras[camIdx].base;

    constexpr f32 visualFar = 15.0f;
    const glm::mat4 visualProj = glm::perspective(cam.fov, aspectRatio, cam.nearPlane, visualFar);
    const glm::mat4 invVP = glm::inverse(visualProj * cam.view);
    // VULKAN NDC REQUIREMENT:
    // X: [-1, 1], Y: [-1, 1], Z: [0, 1]
    AABB ndcVolume;
    ndcVolume.center  = glm::vec3(0.0f, 0.0f, 0.5f); // Center of 0 and 1 is 0.5
    ndcVolume.extents = glm::vec3(1.0f, 1.0f, 0.5f); // Extent from 0.5 to 0 or 1 is 0.5

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
    Renderer::GPUTexture* shadowmap = &swapchain.shadowTexture; // not avaul yet

    cmd.TransitionLayout(shadowmap, TextureLayout::DepthWrite);
    auto shadowDepthAttach = Renderer::RenderAttachment::Depth(shadowmap, LoadOP::Clear, 1.0f);

    constexpr Extent2D shadowExtent = { 2048, 2048 };

    Renderer::RenderingInfo shadowRenderInfo = {
        .extent = shadowExtent,
        .colorAttachments = {}, // Zero color attachments for depth-only pass
        .depthAttachment = &shadowDepthAttach
     };
    cmd.BeginDebugLabel("Shadow Pass", 0.1f, 0.1f, 0.1f);
    cmd.BeginRendering(shadowRenderInfo);
    cmd.SetViewport({0.0f, 0.0f, static_cast<f32>(shadowExtent.width), static_cast<f32>(shadowExtent.height), 0.0f, 1.0f});
    cmd.SetScissor(0, 0, shadowExtent.width, shadowExtent.height);
    cmd.EndRendering();
    cmd.EndDebugLabel();


#ifdef ENABLE_GPU_TIMING
    sceneStats.gpuDrawTime = frame->queryPool.GetElapsedMs(0);
    sceneStats.gpuDrawTime += frame->queryPool.GetElapsedMs(1);
#endif

    cmd.TransitionLayout(colorImage, TextureLayout::ColorWrite);
    cmd.TransitionLayout(depthImage, TextureLayout::DepthWrite);

	auto colorAttach = Renderer::RenderAttachment::Color(colorImage, LoadOP::Clear, {0.1f, 0.1f, 0.1f, 1.0f});
	auto depthAttach = Renderer::RenderAttachment::Depth(depthImage, LoadOP::Clear, 0.0);

	Renderer::RenderingInfo renderInfo = {
		.extent = extent,
		.colorAttachments = SPAN_ONE(colorAttach),
		.depthAttachment = &depthAttach
	};
    cmd.BeginDebugLabel("Scene", 0.2f, 0.8f, 0.2f);
	cmd.BeginRendering(renderInfo);

	cmd.SetViewport({0.0f, 0.0f, static_cast<f32>(extent.width), static_cast<f32>(extent.height), 0.0f, 1.0f});
	cmd.SetScissor(0, 0, extent.width, extent.height);

	const auto cpuStart = std::chrono::high_resolution_clock::now();

    CameraComponent& activeCam = sceneCameras[activeCamIdx];

    cmd.BeginDebugLabel("Models", 0.4f, 0.4f, 0.9f);

#ifdef ENABLE_GPU_TIMING
    frame->queryPool.WriteTimestamp(&cmd, 0);
#endif

    sceneRenderer.PrepareFrame(&windowContext, &activeCam.base, freezeFrustum);
    sceneRenderer.RenderModels(frame->GetCommandBuffer(), frameIndex, sceneStats);
    cmd.EndDebugLabel();

    cmd.BeginDebugLabel("Skybox", 0.6f, 0.3f, 0.6f);
    skybox.Render(&cmd, activeCam.base, aspectRatio);
    cmd.EndDebugLabel();

    cmd.BeginDebugLabel("DebugGizmos", 1.0f, 1.0f, 0.0f);
    QueueFrustumVisualizer(frustumIdx, glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
	if (debugRenderer.enabled)
	{
		debugRenderer.Flush(&cmd, frameIndex);
	}
    cmd.EndDebugLabel();
#ifdef ENABLE_GPU_TIMING
	frame->queryPool.WriteTimestamp(&cmd, 1);
#endif

	const auto cpuEnd = std::chrono::high_resolution_clock::now();
	sceneStats.cpuDrawTime = static_cast<f32>(std::chrono::duration<f64, std::milli>(cpuEnd - cpuStart).count());

	cmd.EndRendering();
    cmd.EndDebugLabel();
}

void Application::RenderImGui(u32 imageIndex)
{
	auto* frame = static_cast<Renderer::VulkanFrameData*>(renderer.GetCurrentFrameData());
	auto& cmd = frame->commandBuffer;

	TracyVkZone(cmd.tracyCtx, cmd.GetVkHandle(), "ImGui");

	const Extent2D extent = swapchain.GetExtent();
	Renderer::GPUTexture* colorImage = swapchain.GetImage(imageIndex);

	Renderer::RenderAttachment colorAttach = Renderer::RenderAttachment::Color(colorImage, LoadOP::Load);

	const Renderer::RenderingInfo renderInfo = {
		.extent = extent,
		.colorAttachments = SPAN_ONE(colorAttach),
	};
    cmd.BeginDebugLabel("UI/ImGui", 0.6f, 0.3f, 0.6f);
	cmd.BeginRendering(renderInfo);

#ifdef ENABLE_GPU_TIMING
    frame->queryPool.WriteTimestamp(&cmd, 2);
#endif

    if (!editorUI.state.noUI)
    {
        CameraComponent& activeCam = sceneCameras[activeCamIdx];

        if (editorUI.state.showMenuBar)
        {
            if (editorUI.DrawMainMenuBar())
            {
                Cleanup();
                return;
            }
        }

        editorUI.DrawCameraGizmo(activeCam);

        editorUI.DrawMainOverlay();

        if (editorUI.state.showEditorTools)
        {
            editorUI.DrawEditorTools();
        }

        if (editorUI.state.showAboutPopup)
        {
            editorUI.AppInfoPopup();
        }

        // --- Transient UI ---
        editorUI.DrawCameraSpeedPopup(cameraSpeedPopupTime);
    }

    editorUI.UpdateLights(windowContext.GetDeltaTime());

	EditorUI::EndFrame();
	EditorUI::Render(frame->GetCommandBuffer());

#ifdef ENABLE_GPU_TIMING
    frame->queryPool.WriteTimestamp(&cmd, 3);
#endif

	cmd.EndRendering();
    cmd.EndDebugLabel();
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
			const ImVec2 win = ImGui::GetWindowSize();
			const ImVec2 sz = ImGui::CalcTextSize(text);
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
				const f32 posX = (x * spacing) - offsetX;
				const f32 posY = (y * spacing) - offsetY + 10.0f;
				const f32 posZ = (z * spacing) - offsetZ - 20.0f; // Push away from camera

				models.push_back(Renderer::ModelComponent{
					.model = sphereMesh.get(),
					.transform = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ)),
					.path = Renderer::RenderPath::Instance,
					// Map Roughness to Y-axis, Metallic to X-axis
					.roughness = std::clamp(static_cast<f32>(y) / (dimY - 1), 0.05f, 1.0f),
					.metallic = std::clamp(static_cast<f32>(x) / (dimX - 1), 0.05f, 1.0f),
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