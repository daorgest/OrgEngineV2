//
// Created by Orgest on 7/13/2025.
//

#include "Application.h"

#include <imgui.h>
#include "glm/gtc/constants.hpp"
#include "glm/gtx/norm.hpp"
#include "Input/InputSys.h"
#include "tracy/Tracy.hpp"
#include "MeshGenerator.h"
#include "MeshLoader.h"
#include "../Engine/ShaderConstants.h"
#include <glm/gtx/transform.hpp>

#include "ShaderCompiler.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "Input/InputSysGameInput.h"


using namespace Renderer;

bool Application::Init()
{
    ZoneScopedN("Application::Init");

    {
        ZoneScopedN("Init Platform & Window");
        Platform::Init(&windowContext);
    }

    // compiler and manager!
    compiler.Init();
    shaderManager.Init(&device, &compiler);

    {
        ZoneScopedN("Init Vulkan Instance, Device, Swapchain, Editor UI, and Command Buffers");
        instance.Init();
        device.Init(&instance);
        CHECK_RESULT(swapchain.Init(&device, windowContext.handle));

        renderer.Init(&device, &swapchain);

        // Editor ui state storage!
        editorUI.state.wc = &windowContext;
        editorUI.state.device = &device;
        editorUI.state.swapchain = &swapchain;
        editorUI.state.debugRenderer = &debugRenderer;
        editorUI.state.sceneStats = &sceneStats;
        editorUI.state.lights = &lights;
        editorUI.state.debugData = &debugData;
        editorUI.state.freezeFrustum = &freezeFrustum;
        editorUI.state.frozenCam = &frozenCamComp;
        editorUI.state.cameraSpeed = cameraSpeed;

        editorUI.Init(&instance, &device, &swapchain);
    }
    // DrawLoadingSplash("Loading...");

    {
        ZoneScopedN("Init Global Descriptor Allocator");
        constexpr u32 initialSets = 100;

        Array sizes = {
            PoolSizes{DescriptorType::SampledImage, static_cast<f32>(MAX_BINDLESS_TEXTURES) / initialSets},
            PoolSizes{DescriptorType::StorageImage, static_cast<f32>(MAX_BINDLESS_TEXTURES) / initialSets},
            PoolSizes{DescriptorType::UniformBuffer, 4.0f},
            PoolSizes{DescriptorType::StorageBuffer, 10.0f},
            PoolSizes{DescriptorType::CombinedImageSampler, 10.0f},
        };
        globalDescriptorAlloc.Init(&device, initialSets, sizes);
    }

    {
        ZoneScopedN("Init Scene UBO");
        DescriptorSetLayoutDesc sceneDesc = {
            DescriptorSetLayoutDesc::FromConstants(0, Constants::Scene),
        };
        sceneUBO = std::make_unique<Renderer::VulkanShaderBuffer>(&device, &globalDescriptorAlloc, sceneDesc);
    }

    // Debug Renderer
    debugRenderer.Initialize(&device, sceneUBO.get(), &globalDescriptorAlloc);

    // Bindless Manager
    bindlessManager.Init(&device, texturePool, globalDescriptorAlloc);

    Platform::ShowWindow(windowContext);

    // Main model
    {
            ZoneScopedN("LoadModel");
            RenderLoadingSplash("Loading Model...");
            LOG(Debug, "Loading Model...");

            const auto sponzaHandle = modelPool.Load("bistro/bistro.gltf", [&](const std::string& path) -> Result<Renderer::GPUModel>
            {
                return Assets::MeshLoader::LoadModelFromSource(MeshSourceType::GLTF, path, &texturePool)
                    .and_then([&](auto&& loadedModel) {
                        // SUCCESS PATH: Push the loaded model to the GPU
                        return Renderer::CreateVulkanModel(&device, loadedModel, bindlessManager, globalDescriptorAlloc);
                    })
                    .or_else([&](OrgErrCode err) -> Result<Renderer::GPUModel> {
                        LOG(Warning, "Failed to load '{}' (Error Code: {}). Using fallback cube.", path, static_cast<i32>(err));

                        LoadedModel fallbackModel;
                        fallbackModel.sourceType = MeshSourceType::Runtime;

                        Mesh fallbackMesh = MeshGenerator::GenerateCube(2.0f);
                        fallbackMesh.name = "ErrorCube";
                        fallbackModel.meshes.push_back(std::move(fallbackMesh));

                        Material fallbackMat;
                        fallbackMat.name = "ErrorMaterial";
                        fallbackMat.baseColor = glm::vec3(1.0f, 0.0f, 1.0f); // Bright Magenta!
                        fallbackMat.roughness = 0.9f;
                        fallbackMat.metallic = 0.0f;
                        fallbackModel.materials.push_back(fallbackMat);

                        return Renderer::CreateVulkanModel(&device, fallbackModel, bindlessManager, globalDescriptorAlloc);
                    });
            });

            if (sponzaHandle)
            {
                MaterialComponent mat = {
                    .materialIndex = 0,
                    .roughness = 1.0f,
                    .metallic = 0.0f,
                    .tint = glm::vec3(1.0f)
                };

                AddEntity(*sponzaHandle, glm::mat4(1.0f), RenderPath::Standard, mat);
            }

            texturePool.Clear();
        }

    // DrawLoadingSplash("Building Skybox...");

    if (!skybox.Initialize(&device, globalDescriptorAlloc))
    {
        LOG(Warning, "Skybox initialization failed - Skybox will not be rendered");
    }

    // RenderLoadingSplash("Creating PBR Sphere Grid...");
    // CreatePBRSphereGrid();

    // 1 Light!
    {
        constexpr f32 lightIntensity = 11.0f;
        Engine::LightUBO L{};
        L.type = Engine::LightType::Directional;
        L.position = {0.0f, 0, 0};
        L.direction = { 0.8f, -0.56f, 0.0f};
        L.color = {1.0f, 0.990f, 0.625f};
        L.intensity = lightIntensity;
        lights.push_back(L);
    }

    lightUBO.count = static_cast<u32>(std::min(lights.size(), static_cast<size_t>(16)));

    sceneTargets.Resize(&device, swapchain.GetExtent(), device.currentSamples);
    // Initialize Scene Renderer - abstracts all model rendering logic
    SceneRenderConfig renderConfig = {
        .device = &device,
        .swapchain = &swapchain,
        .descriptorAllocator = &globalDescriptorAlloc,
        .bindless = &bindlessManager,
        .shaderManager = &shaderManager,
        .modelPool = &modelPool,
        .sceneUBO = sceneUBO.get(),
        .skybox = &skybox,
        .debugRenderer = &debugRenderer,
        .entityModels = &entityModels,
        .entityTransforms = &entityTransforms,
        .entityPaths = &entityPaths,
        .entityMaterials = &entityMaterials
    };
    sceneRenderer.Init(renderConfig);

    ComputeStaticSceneStats();
    computeDemo.Init(&device, &shaderManager, globalDescriptorAlloc);

    for (CameraComponent& camera : sceneCameras)
    {
        camera.position = {0.0f, 0, 0};

        // 1. Set angles on the CONTROLLER
        camera.controller.yaw = -90.0f;
        camera.controller.pitch = 0.0f;

        // Controller Defaults
        camera.controller.fovBase = 70.0f;
        camera.controller.eyeHeight = 1.6f;

        glm::quat qYaw = glm::angleAxis(Radians(camera.controller.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat qPitch = glm::angleAxis(Radians(camera.controller.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        camera.camera.rotation = qYaw * qPitch;


        camera.camera.Update(camera.position, aspectRatio);
    }
    camMode = CameraMode::FreeFly;
    editorUI.state.cameraComponents = sceneCameras;
    editorUI.state.activeCameraIdx = activeCamIdx;
    InitDefaultKeyBindings();

    InitDefaultKeyBindings();

    debugData.debugMode = Renderer::DebugView::Material;


    debugData.iblStrength = 0.7f;
    debugData.ambientStrength = 0.08f;
    debugData.aoStrength = 1.3f;
    debugData.metallicReflectScale = 0.8f;
    debugData.roughnessReflectScale = 0.9f;

    debugData.shadowTint = glm::vec3(0.4f, 0.45f, 0.55f);
    return true;
}

u32 Application::AddEntity(const ResourceHandle<GPUModel> model, const glm::mat4& transform, const RenderPath path,
                           const MaterialComponent& mat)
{
    entityModels.push_back(model);
    entityTransforms.push_back({transform});
    entityPaths.push_back({path});
    entityMaterials.push_back(mat);
    return static_cast<u32>(entityModels.size() - 1);
}

void Application::Run()
{
    while (Platform::ProcessMessages(&windowContext))
    {
        FrameMarkStart("Frame");

        // Input
        {
            ZoneScopedN("GameInput Update");
#if ENGINE_PLATFORM_WIN32
            gameInput.Update(windowContext);
#endif
        }

        // Audio
        {
            ZoneScopedN("Sound System Update");
        }

        if (editorUI.state.autoReloadShaders || editorUI.state.pendingManualReload)
        {
            shaderManager.CheckForReloads();
            editorUI.state.pendingManualReload = false;
        }

        Platform::StartFrame(windowContext);

        if (!swapchain.ResizeIfNeeded() || windowContext.displayState.isMinimized)
        {
            FrameMarkEnd("Frame");
            continue;
        }

        if (swapchain.justRecreated)
        {
            sceneTargets.Resize(&device, swapchain.GetRenderExtent(), device.currentSamples);

            // editorUI.state.pendingMSAAChange = false;
            swapchain.justRecreated = false;
        }

        // Runtime msaa!!
        if (editorUI.state.pendingMSAAChange)
        {
            sceneTargets.Resize(&device, swapchain.GetRenderExtent(), device.currentSamples);

            sceneRenderer.GetOpaquePipeline()->SetSampleCountAndRebuild(device.currentSamples);
            sceneRenderer.GetTransparentPipeline()->SetSampleCountAndRebuild(device.currentSamples);
            skybox.GetPipeline()->SetSampleCountAndRebuild(device.currentSamples);
            debugRenderer.GetPipeline()->SetSampleCountAndRebuild(device.currentSamples);

            editorUI.state.pendingMSAAChange = false;
        }

        if (renderer.BeginFrame())
        {
            {
                ZoneScopedN("Update Logic");
                UpdateCamera();
                UpdateSceneUBO();
            }

            {
                ZoneScopedN("Render Logic");
                RenderScene();
                RenderImGui();
            }

            {
                ZoneScopedN("EndFrame (Present)");
                renderer.EndFrame();
            }
        }


        Input::EndFrameInputUpdate();
        FrameMarkEnd("Frame");
    }
}

void Application::RenderLoadingSplash(const char* text)
{
    // Process a frame just like Run() does, but only once.
    if (!swapchain.ResizeIfNeeded()) return;
    GPUCommandBuffer* cmd = renderer.BeginFrame();
    if (!cmd) return;

    GPUTexture* colorImage = swapchain.GetCurrentImage();
    const Extent2D extent = swapchain.GetExtent();

    cmd->TransitionLayout(colorImage, TextureLayout::ColorWrite);

    cmd->FlushBarriers();

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

    auto colorAttach = Renderer::RenderAttachment::Color(colorImage->GetView());

    Renderer::RenderingInfo renderInfo = {
        .extent = extent,
        .colorAttachments = SPAN_ONE(colorAttach),
    };

    cmd->BeginRendering(renderInfo);

    editorUI.EndFrame();
    editorUI.Render(cmd);

    cmd->EndRendering();

    cmd->TransitionLayout(colorImage, TextureLayout::Present);
    cmd->FlushBarriers();
    renderer.EndFrame();
}

void Application::RenderScene()
{
    auto* frame = renderer.GetCurrentFrameData();
    auto* cmd = frame->GetCommandBuffer();
    const u32 frameIndex = renderer.GetFrameIndex();
    auto* queryPool = frame->GetQueryPool();

    const Renderer::Extent2D renderRes = {swapchain.renderWidth, swapchain.renderHeight};
    const bool isMSAAEnabled = (device.currentSamples != Renderer::SampleCount::X1);

    GPUTexture* msaa = sceneTargets.GetMSAA();
    GPUTexture* sceneColor = sceneTargets.GetSceneColor();
    GPUTexture* depth = sceneTargets.GetDepth();

    if (isMSAAEnabled && msaa)
    {
        cmd->TransitionLayout(msaa, TextureLayout::ColorWrite);
    }
    cmd->TransitionLayout(sceneColor, TextureLayout::ColorWrite);
    cmd->TransitionLayout(depth, TextureLayout::DepthWrite);

    cmd->FlushBarriers();

    Renderer::RenderAttachment colorAttach;
    if (isMSAAEnabled && msaa)
    {
        colorAttach = Renderer::RenderAttachment::Color(msaa->GetView(), sceneColor->GetView());
    }
    else
    {
        colorAttach = Renderer::RenderAttachment::Color(sceneColor->GetView(), nullptr);
    }

    auto depthAttach = Renderer::RenderAttachment::Depth(depth->GetView());


    const Renderer::RenderingInfo renderInfo = {
        .extent = renderRes,
        .colorAttachments = SPAN_ONE(colorAttach),
        .depthAttachment = &depthAttach
    };

    // {
    //     Renderer::GPUTexture* shadowMap = &swapchain.shadowTexture; // not available yet
    //     cmd.TransitionLayout(shadowMap, TextureLayout::DepthWrite);
    //        auto shadowDepthAttach = Renderer::RenderAttachment::Depth(shadowMap, LoadOP::Clear, 1.0f);
    //
    //        constexpr Extent2D shadowExtent = { 2048, 2048 };
    //
    //        Renderer::RenderingInfo shadowRenderInfo = {
    //            .extent = shadowExtent,
    //            .colorAttachments = {}, // Zero color attachments for depth-only pass
    //            .depthAttachment = &shadowDepthAttach
    //         };
    //        cmd.BeginDebugLabel("Shadow Pass", 0.1f, 0.1f, 0.1f);
    //        cmd.BeginRendering(shadowRenderInfo);
    //        cmd.SetViewport({0.0f, 0.0f, static_cast<f32>(shadowExtent.width), static_cast<f32>(shadowExtent.height), 0.0f, 1.0f});
    //        cmd.SetScissor(0, 0, shadowExtent.width, shadowExtent.height);
    //        cmd.EndRendering();
    //        cmd.EndDebugLabel();
    // }

    cmd->BeginDebugLabel("Scene", 0.2f, 0.8f, 0.2f);
    cmd->BeginRendering(renderInfo);

    cmd->SetViewport({0.0f, 0.0f, (f32)renderRes.width, (f32)renderRes.height, 0.0f, 1.0f});
    cmd->SetScissor(0, 0, renderRes.width, renderRes.height);

    const auto cpuStart = std::chrono::high_resolution_clock::now();

    const CameraComponent& activeCam = sceneCameras[activeCamIdx];
    const Camera& cullingCam = freezeFrustum ? frozenCamComp.camera : activeCam.camera;

    cmd->BeginDebugLabel("Models", 0.4f, 0.4f, 0.9f);

#ifdef ENABLE_GPU_TIMING
    queryPool->WriteTimestamp(cmd, 0);
#endif

    sceneRenderer.PrepareFrame(&windowContext, &cullingCam);
    sceneRenderer.RenderModels(cmd, frameIndex, sceneStats);
    cmd->EndDebugLabel();

    cmd->BeginDebugLabel("Skybox", 0.6f, 0.3f, 0.6f);
    skybox.Render(cmd, activeCam.camera);
    cmd->EndDebugLabel();

    cmd->BeginDebugLabel("DebugGizmos", 1.0f, 1.0f, 0.0f);
    QueueFrustumVisualizer(frustumIdx, glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));

    if (debugRenderer.enabled)
    {
        debugRenderer.Flush(cmd, frameIndex);
    }
    cmd->EndDebugLabel();
#ifdef ENABLE_GPU_TIMING
    queryPool->WriteTimestamp(cmd, 1);
#endif

    const auto cpuEnd = std::chrono::high_resolution_clock::now();
    sceneStats.cpuDrawTime = static_cast<f32>(std::chrono::duration<f64, std::milli>(cpuEnd - cpuStart).count());

    cmd->EndRendering();
    cmd->EndDebugLabel();
}

void Application::RenderImGui()
{
    auto* frame = renderer.GetCurrentFrameData();
    auto* cmd = frame->GetCommandBuffer();
    const Extent2D nativeRes = swapchain.GetExtent();
    GPUTexture* swapchainImage = swapchain.GetCurrentImage();
    auto* queryPool = frame->GetQueryPool();

    GPUTexture* sceneColor = sceneTargets.GetSceneColor();

    cmd->TransitionLayout(sceneColor, TextureLayout::CopySource);
    cmd->TransitionLayout(swapchainImage, TextureLayout::CopyDestination);
    cmd->FlushBarriers();

    if (swapchain.renderScale == 1.0f)
    {
        cmd->CopyTexture(sceneColor, swapchainImage);
    }
    else
    {
        cmd->BlitTexture(sceneColor, swapchainImage);
    }

    cmd->TransitionLayout(swapchainImage, TextureLayout::ColorWrite);

    cmd->FlushBarriers();

    auto colorAttach = Renderer::RenderAttachment::Color(swapchainImage->GetView(), nullptr, LoadOP::Load);

    const Renderer::RenderingInfo renderInfo = {
        .extent = nativeRes,
        .colorAttachments = SPAN_ONE(colorAttach),
    };


#ifdef ENABLE_GPU_TIMING

    // Slot 0-1: Scene Pass | Slot 2-3: UI Pass
    sceneStats.gpuDrawTime = queryPool->GetElapsedMs(0) + queryPool->GetElapsedMs(1);

    const f32 frameMs = (windowContext.frameTime > 0.0f)
                            ? windowContext.frameTime
                            : (1000.0f / windowContext.fps);

    const f32 busy = (sceneStats.gpuDrawTime / frameMs) * 100.0f;
    sceneStats.gpuBusy = std::clamp(busy, 0.0f, 100.0f);


    queryPool->WriteTimestamp(cmd, 2);
#endif

    cmd->BeginDebugLabel("UI/ImGui", 0.6f, 0.3f, 0.6f);
    EditorUI::BeginFrame();

    if (!editorUI.state.noUI)
    {
        computeDemo.DrawUI(&device);

        const ComputePushConstants cPC = {
            .time = static_cast<f32>(windowContext.elapsedTime),
            .mousePos = {ImGui::GetMousePos().x, ImGui::GetMousePos().y}
        };
        computeDemo.Execute(cmd, computeDemo.lastExtent, cPC);
    }

    cmd->BeginRendering(renderInfo);
    cmd->SetViewport({0.0f, 0.0f, static_cast<f32>(nativeRes.width), static_cast<f32>(nativeRes.height), 0.0f, 1.0f});

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

        // ImGui::ShowDemoWindow();

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

        editorUI.DrawDisplaySettings();

        // --- Transient UI ---
        editorUI.DrawDebugViewPopup(debugViewPopupTime, debugData.debugMode);
        editorUI.DrawCameraSpeedPopup(cameraSpeedPopupTime);
    }

    editorUI.UpdateLights();

    EditorUI::EndFrame();
    EditorUI::Render(cmd);

#ifdef ENABLE_GPU_TIMING
    queryPool->WriteTimestamp(cmd, 3);
#endif

    cmd->EndRendering();

    cmd->TransitionLayout(swapchainImage, TextureLayout::Present);
    cmd->FlushBarriers();
    cmd->EndDebugLabel();
}

void Application::DumpVmaLeaksToFile(const char* filename) const
{
    char* statsString = nullptr;
    // vmaBuildStatsString generates a JSON report of all currently active allocations
    vmaBuildStatsString(device.allocator, &statsString, VK_TRUE);

    if (statsString)
    {
        FILE* file = fopen(filename, "w");
        if (file)
        {
            fprintf(file, "%s", statsString);
            fclose(file);
            LOG(Info, "VMA leak report successfully written to: {}", filename);
        }
        else
        {
            LOG(Error, "Failed to open file for VMA leak report: {}", filename);
        }
        vmaFreeStatsString(device.allocator, statsString);
    }
}

void Application::Cleanup()
{
    device.WaitIdle();
    computeDemo.Destroy();
    skybox.Cleanup();
    debugRenderer.Cleanup();
    sceneRenderer.Destroy();
    sceneUBO.reset();

    texturePool.Clear();
    modelPool.Clear();

    renderer.Destroy();
    bindlessManager.Destroy();
    shaderManager.Destroy();
    compiler.Destroy();

    swapchain.Destroy();

    DumpVmaLeaksToFile("vma_leaks_final.json");
}

void Application::InitDefaultKeyBindings()
{
    // --- Rebindable Movement (Keyboard + Gamepad) ---
    input.BindAction(Action::MoveForward, Keyboard::W);
    input.BindAction(Action::MoveBackward, Keyboard::S);
    input.BindAction(Action::MoveLeft, Keyboard::A);
    input.BindAction(Action::MoveRight, Keyboard::D);
    input.BindAction(Action::MoveUp, Keyboard::E);
    input.BindAction(Action::MoveDown, Keyboard::Q);
    input.BindAction(Action::Jump, Keyboard::Space);
    input.BindAction(Action::Crouch, Keyboard::Ctrl);
    input.BindAction(Action::Sprint, Keyboard::Shift);


    // Additive Gamepad bindings for movement
    input.BindAction(Action::MoveForward, Gamepad::Button::DpadUp);
    input.BindAction(Action::MoveBackward, Gamepad::Button::DpadDown);
    input.BindAction(Action::MoveLeft, Gamepad::Button::DpadLeft);
    input.BindAction(Action::MoveRight, Gamepad::Button::DpadRight);
    input.BindAction(Action::Jump, Gamepad::Button::X);
    input.BindAction(Action::Crouch, Gamepad::Button::R3);
    input.BindAction(Action::Sprint, Gamepad::Button::L3);

    // --- Strict Keyboard System Keys (F1–F9) ---
    input.BindAction(Action::ToggleFPS, Keyboard::F1);
    input.BindAction(Action::ToggleDebug, Keyboard::F2);
    input.BindAction(Action::ToggleMenuBar, Keyboard::F3);
    input.BindAction(Action::ToggleGPUInfo, Keyboard::F4);
    input.BindAction(Action::ToggleVSync, Keyboard::F5);
    input.BindAction(Action::ToggleUI, Keyboard::F6);
    input.BindAction(Action::ToggleFrustum, Keyboard::F7);
    input.BindAction(Action::CycleCamera, Keyboard::F8);
    input.BindAction(Action::CycleDebugView, Keyboard::F9);
}

void Application::ComputeStaticSceneStats()
{
    ZoneScopedN("ComputeStaticSceneStats"); // Add profiling for this pass

    sceneStats.totalVerts = 0;
    sceneStats.totalTris = 0;
    sceneStats.totalMeshCount = 0;

    for (const auto& modelHandle : entityModels)
    {
        auto modelRes = modelPool.Get(modelHandle);
        if (!modelRes) continue;

        const auto* model = *modelRes;


        if (model->vertexBuffer && model->vertexBuffer->IsValid())
        {
            sceneStats.totalVerts += static_cast<u32>(model->vertexBuffer->GetSize() / sizeof(Vertex));
        }


        for (const auto& part : model->parts)
        {
            sceneStats.totalTris += (part.indexCount / 3);
            ++sceneStats.totalMeshCount;
        }
    }

    const Extent2D extent = swapchain.GetExtent();
    LOG(Debug, "Scene Stats - Triangles: {} | Verts: {} | MeshParts: {} | Resolution: {}x{}",
        sceneStats.totalTris, sceneStats.totalVerts, sceneStats.totalMeshCount,
        extent.width, extent.height);
}

void Application::UpdateSceneUBOAtIndex(u32 frameIndex) const
{
    if (sceneUBO)
    {
        sceneUBO->UpdateBinding(frameIndex, 2, &debugData, sizeof(Engine::DebugUBO));
        sceneUBO->UpdateBinding(frameIndex, 3, &camUBO, sizeof(Engine::CameraUBO));
        sceneUBO->UpdateBinding(frameIndex, 4, &lightUBO, sizeof(Engine::LightSceneData));
        sceneUBO->UpdateBinding(frameIndex, 6, &sceneData, sizeof(Engine::SceneUBO));
    }
}

void Application::UpdateSceneUBO()
{
    ZoneScopedN("UpdateSceneUBO");
    const CameraComponent& activeCam = sceneCameras[activeCamIdx];
    glm::vec3 finalEyePos = activeCam.position;
    if (camMode == CameraMode::FPS)
    {
        finalEyePos.y += activeCam.controller.eyeHeight;
    }

    aspectRatio = static_cast<f32>(swapchain.width) / static_cast<f32>(swapchain.height);

    sceneData.view = activeCam.camera.view;
    sceneData.proj = activeCam.camera.projection;
    camUBO.position = finalEyePos;
    camUBO.nearPlane = activeCam.camera.nearPlane;
    camUBO.farPlane = activeCam.camera.farPlane;

    if (editorUI.state.spinLights && !lights.empty())
    {
        const f32 dt = windowContext.GetDeltaTime();
        editorUI.state.currentLightTime += dt * editorUI.state.spinSpeed;

        const u32 spinCount = std::min((u32)lights.size(), 4u);
        for (u32 i = 0; i < spinCount; ++i)
        {
            const f32 angle = editorUI.state.currentLightTime + (static_cast<f32>(i) * (glm::two_pi<f32>() /
                spinCount));
            lights[i].position = glm::vec3(
                editorUI.state.spinCenter.x + (editorUI.state.spinRadius * std::sin(angle)),
                editorUI.state.spinHeight,
                editorUI.state.spinCenter.z + (editorUI.state.spinRadius * std::cos(angle))
            );

            if (lights[i].type == Engine::LightType::Spot)
                lights[i].direction = glm::normalize(editorUI.state.spinCenter - lights[i].position);
        }
    }


    lightUBO.count = static_cast<u32>(lights.size());

    // 2. Safely transfer and transform the data in-flight!
    for (size_t i = 0; i < lights.size(); i++)
    {
        // Copy the base data (Position, Color, Intensity, etc.)
        lightUBO.lights[i] = lights[i];

        // 3. If it's a Spotlight, mathematically convert the UI's degrees into Cosines for the HLSL shader
        if (lights[i].type == Engine::LightType::Spot)
        {
            lightUBO.lights[i].innerCone = std::cos(glm::radians(lights[i].innerCone));
            lightUBO.lights[i].outerCone = std::cos(glm::radians(lights[i].outerCone));
        }
    }

    TracyPlot("Draw Calls", static_cast<i64>(sceneStats.drawCallCount));
    TracyPlot("Triangle Count", static_cast<i64>(sceneStats.totalTris));
    TracyPlot("GPU Draw Time (ms)", sceneStats.gpuDrawTime);

    UpdateSceneUBOAtIndex(renderer.GetFrameIndex());
}

void Application::ApplyFreeFlyMovement(const u32 idx, const f32 dt)
{
    CameraComponent& camComp = sceneCameras[idx];
    Camera& cam = camComp.camera;
    FPSCamera& ctrl = camComp.controller;
    glm::vec3& worldPos = camComp.position;

    f32 deltaYaw = 0.0f;
    f32 deltaPitch = 0.0f;

    const glm::vec3 camFwd = cam.GetForward();
    const glm::vec3 camRight = cam.GetRight();
    const glm::vec3 camUp = cam.GetUp();

    const bool isPanning = input.IsKeyHeld(Keyboard::Alt) && input.IsMouseHeld(Mouse::Middle);
    const bool isLooking = input.IsMouseHeld(Mouse::Right);
    const bool shouldCaptureMouse = isPanning || isLooking;

    static bool lastCaptureState = false;
    if (shouldCaptureMouse != lastCaptureState)
    {
        Platform::SetCursorLocked(&windowContext, shouldCaptureMouse);
        Platform::SetCursorVisible(&windowContext, !shouldCaptureMouse);
        lastCaptureState = shouldCaptureMouse;
    }
    if (isPanning)
    {
        const f32 panSpeed = cameraSpeed * 0.001f;
        worldPos += (camRight * static_cast<f32>(-input.xrel) + camUp * static_cast<f32>(input.yrel)) * panSpeed;
        input.scrollY = 0; // Lock scroll speed adjustment while panning
    }
    else if (isLooking)
    {
        deltaYaw -= static_cast<f32>(input.xrel) * 0.1f;
        deltaPitch -= static_cast<f32>(input.yrel) * 0.1f;
        Platform::WrapCursorToOppositeEdge(&windowContext);
    }

    // Controller movement
    if (input.usingController && input.controllers[0].connected)
    {
        const f32 rightX = input.GetRightStickX();
        const f32 rightY = input.GetRightStickY();

        // Only apply math if sticks are actually outside the deadzone
        if (rightX != 0.0f || rightY != 0.0f)
        {
            deltaYaw -= rightX * 150.0f * dt;
            deltaPitch += rightY * 150.0f * dt;
        }
    }

    if (deltaYaw != 0.0f || deltaPitch != 0.0f)
    {
        ctrl.yaw += deltaYaw;
        ctrl.pitch = std::clamp(ctrl.pitch + deltaPitch, -89.0f, 89.0f);

        const glm::quat qYaw = glm::angleAxis(Radians(ctrl.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat qPitch = glm::angleAxis(Radians(ctrl.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        cam.rotation = qYaw * qPitch;
    }
    glm::vec3 move{0.0f};

    // Use the quaternion-derived direction vectors
    if (input.IsActionHeld(Action::MoveForward)) move += cam.GetForward();
    if (input.IsActionHeld(Action::MoveBackward)) move -= cam.GetForward();
    if (input.IsActionHeld(Action::MoveLeft)) move -= cam.GetRight();
    if (input.IsActionHeld(Action::MoveRight)) move += cam.GetRight();
    if (input.IsActionHeld(Action::MoveUp)) move += cam.GetUp();
    if (input.IsActionHeld(Action::MoveDown)) move -= cam.GetUp();

    if (input.usingController && input.controllers[0].connected)
    {
        const f32 leftX = input.GetLeftStickX();
        const f32 leftY = input.GetLeftStickY();

        if (leftX != 0.0f || leftY != 0.0f)
        {
            move -= camFwd * leftY;
            move -= camRight * leftX;
        }
    }

    if (glm::length2(move) > 1e-6f)
    {
        worldPos += glm::normalize(move) * cameraSpeed * dt;
    }
}

void Application::UpdateCamera()
{
    if (!windowContext.displayState.isFocused) return;

    activeCamIdx = editorUI.state.activeCameraIdx;
    const f32 dt = std::min(windowContext.GetDeltaTime(), 0.100f);

    CameraComponent& activeCam = sceneCameras[activeCamIdx];

    // F1: Toggle between FPS and FreeFly mode
    if (input.IsActionDown(Action::ToggleFPS))
    {
        const bool toFPS = (camMode == CameraMode::FreeFly);
        camMode = toFPS ? CameraMode::FPS : CameraMode::FreeFly;

        const f32 offset = activeCam.controller.eyeHeight;
        activeCam.position.y += toFPS ? -offset : offset;

        if (camMode == CameraMode::FPS)
        {
            activeCam.controller.velocity = glm::vec3(0.0f);
            activeCam.controller.grounded = false;
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
    if (input.IsActionDown(Action::ToggleFrustum))
    {
        freezeFrustum = !freezeFrustum;
        if (freezeFrustum)
        {
            frozenCamComp = sceneCameras[activeCamIdx];
            frustumIdx = activeCamIdx;
        }
    }
    if (input.IsActionDown(Action::CycleCamera))
    {
        activeCamIdx = (activeCamIdx + 1) % MAX_SCENE_CAMERAS;
        selectedCameraIdx = activeCamIdx;
        editorUI.state.selectedCameraIdx = selectedCameraIdx;
        editorUI.state.activeCameraIdx = activeCamIdx;
    }
    if (input.IsActionDown(Action::CycleDebugView))
    {
        debugData.debugMode = static_cast<DebugView>((static_cast<i32>(debugData.debugMode) + 1) % kDebugViewCount);
        debugViewPopupTime = 1.5f;
    }

    glm::vec3 renderPos = activeCam.position;
    if (camMode == CameraMode::FPS)
    {
        const bool wantsUnlock = input.IsKeyHeld(Keyboard::Alt);

        if (windowContext.displayState.isFocused)
        {
            Platform::SetCursorLocked(&windowContext, !wantsUnlock);
            Platform::SetCursorVisible(&windowContext, wantsUnlock);
        }

        if (!wantsUnlock)
        {
            Platform::CenterMouse(&windowContext);
            activeCam.controller.Update(activeCam, dt);
        }

        glm::vec3 bobOffset{0.0f};

        // FIX: Check walkLerp instead of headTimer
        if (activeCam.controller.walkLerp > 0.001f)
        {
            const f32 s = std::sin(activeCam.controller.headTimer * glm::two_pi<f32>());
            const f32 c = std::cos(activeCam.controller.headTimer * glm::two_pi<f32>());

            bobOffset = activeCam.camera.GetRight() * (s * activeCam.controller.tune.bobHorizAmp);
            bobOffset.y = std::abs(c * activeCam.controller.tune.bobVertAmp);
            bobOffset *= activeCam.controller.walkLerp;
        }

        renderPos = activeCam.position + glm::vec3(0.0f, activeCam.controller.eyeHeight, 0.0f) + bobOffset;
    }
    else
    {
        ApplyFreeFlyMovement(activeCamIdx, dt);
        renderPos = activeCam.position;
    }

    // Scroll Wheel Speed!
    if (input.scrollY != 0.0f && windowContext.displayState.isFocused && camMode == CameraMode::FreeFly)
    {
        // A scroll of +2.0 multiplies by 1.1^2 (1.21x).
        // A scroll of -1.0 multiplies by 1.1^-1 (~0.9x).
        // This makes fast scrolling infinitely smoother and more responsive!
        const f32 multiplier = std::pow(1.1f, input.scrollY);
        cameraSpeed = std::clamp(cameraSpeed * multiplier, 0.1f, 500.0f);

        editorUI.state.cameraSpeed = cameraSpeed;
        cameraSpeedPopupTime = 1.5f; // Reset the timer while scrolling
    }

    cameraSpeedPopupTime = std::max(0.0f, cameraSpeedPopupTime - dt);
    debugViewPopupTime = std::max(0.0f, debugViewPopupTime - dt);

    activeCam.camera.Update(renderPos, aspectRatio);

    if (!freezeFrustum) frustumIdx = activeCamIdx;
    selectedCameraIdx = editorUI.state.selectedCameraIdx;
}

void Application::QueueFrustumVisualizer(u32 camIdx, const glm::vec4& color)
{
    if (!debugRenderer.enabled) return;
    if (camIdx == activeCamIdx && !freezeFrustum) return;

    const Camera& cam = freezeFrustum ? frozenCamComp.camera : sceneCameras[camIdx].camera;

    constexpr f32 visualFar = 25.0f;
    const glm::mat4 visualProj = glm::perspectiveRH_ZO(Radians(cam.fov), aspectRatio, cam.nearPlane, visualFar);
    const glm::mat4 invVP = glm::inverse(visualProj * cam.view);
    // VULKAN NDC REQUIREMENT:
    // X: [-1, 1], Y: [-1, 1], Z: [0, 1]
    AABB ndcVolume;
    ndcVolume.center = glm::vec3(0.0f, 0.0f, 0.5f); // Center of 0 and 1 is 0.5
    ndcVolume.extents = glm::vec3(1.0f, 1.0f, 0.5f); // Extent from 0.5 to 0 or 1 is 0.5

    debugRenderer.SetColor(color);
    debugRenderer.QueueBox(invVP, ndcVolume, color);
}

void Application::CreatePBRSphereGrid()
{
    ZoneScopedN("CreatePBRSphereGrid3D");

    auto sphereHandleRes = modelPool.Load("runtime://pbr_sphere",
                                          [&]([[maybe_unused]] const std::string& path) -> Result<Renderer::GPUModel>
                                          {
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

                                              pbrMat.albedoPath = "engine://white";

                                              loadedModel.materials.push_back(pbrMat);

                                              return CreateVulkanModel(&device, loadedModel, bindlessManager,
                                                                       globalDescriptorAlloc);
                                          });

    if (!sphereHandleRes)
    {
        LOG(Error, "Failed to create PBR Sphere model grid!");
        return;
    }

    // Extract the safe handle from the result
    const auto sphereHandle = *sphereHandleRes;

    // 3D Grid Dimensions
    constexpr i32 dimX = 10; // Metallic variation
    constexpr i32 dimY = 10; // Roughness variation
    constexpr i32 dimZ = 10; // Depth stacking
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
                const f32 posZ = (z * spacing) - offsetZ - 20.0f;


                Renderer::MaterialComponent mat = {
                    .materialIndex = 0,
                    .roughness = std::clamp(static_cast<f32>(y) / (dimY - 1), 0.05f, 1.0f),
                    .metallic = std::clamp(static_cast<f32>(x) / (dimX - 1), 0.05f, 1.0f)
                };

                AddEntity(sphereHandle,
                          glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ)),
                          RenderPath::Instance,
                          mat);
            }
        }
    }
}
