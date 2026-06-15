  //
// Created by Orgest on 7/13/2025.
//

#pragma once

#include <future>

#include "BindlessManager.h"
#include "Camera.h"
#include "ComputeDemonstration.h"
#include "DebugRenderer.h"
#include "EditorUi.h"
#include "Platform.h"
#include "RenderResources.h"
#include "SceneRenderer.h"
#include "ShaderCompiler.h"
#include "ShaderParams.h"
#include "SkyboxManager.h"
#include "VulkanCommands.h"
#include "VulkanDevice.h"
#include "VulkanInit.h"
#include "VulkanMesh.h"
#include "VulkanShaderBuffer.h"
#include "VulkanShaderManager.h"
#include "VulkanSwapchain.h"
#include "../Engine/MeshStats.h"

// Forward declaration to break circular dependency with EditorUi.h
struct EditorUI;

struct Application
{


    Application() = default;

    ~Application() { device.WaitIdle(); }
    Platform::WindowContext windowContext;

	Renderer::VulkanInstance instance;
	Renderer::VulkanDevice device;
	Renderer::VulkanSwapchain swapchain;
	Renderer::VulkanRenderer renderer;
	Renderer::DescriptorAllocatorGrowable globalDescriptorAlloc;
    Renderer::SceneRenderTargets sceneTargets;
	SkyboxManager skybox;
	DebugRenderer debugRenderer;
	SceneRenderer sceneRenderer;
    ComputeDemonstration computeDemo;

    Renderer::ShaderCompiler compiler;
    Renderer::VulkanShaderManager shaderManager;
    // Audio::System audioSys;
    EditorUI editorUI;

    AssetPool<Renderer::TextureData> texturePool;
    AssetPool<Renderer::GPUModel> modelPool;
    Renderer::BindlessManager bindlessManager;

    Vector<ResourceHandle<Renderer::GPUModel>> entityModels;
    Vector<Renderer::TransformComponent>       entityTransforms;
    Vector<Renderer::RenderPathComponent>      entityPaths;
    Vector<Renderer::MaterialComponent>        entityMaterials;

    CameraMode camMode = CameraMode::FreeFly;
    Array<CameraComponent, MAX_SCENE_CAMERAS> sceneCameras;
    CameraComponent frozenCamComp;

    u32 activeCamIdx = 0;
    u32 selectedCameraIdx = 0;
    u32 frustumIdx = 0;


    std::unique_ptr<Renderer::VulkanShaderBuffer> sceneUBO;
	Engine::SceneUBO sceneData;
	Engine::DebugUBO debugData;
	Engine::CameraUBO camUBO;
	Engine::LightSceneData lightUBO;
    Vector<Engine::LightUBO> lights;
	SceneStats sceneStats;

	f32 aspectRatio;
	f32 cameraSpeed = 5;
	f32 cameraSpeedPopupTime = 0.0f;
    f32 debugViewPopupTime = 0.0f;
	f32 lastFrameMs = 0.0f;
	bool freezeFrustum = false;

	bool Init();
    u32 AddEntity(ResourceHandle<Renderer::GPUModel> model, const glm::mat4& transform, Renderer::RenderPath path, const Renderer::MaterialComponent& mat);
    void Run();
    void RenderLoadingSplash(const char* text);
    void RenderScene();
    void RenderImGui();
    void DumpVmaLeaksToFile(const char* filename) const;
    void Cleanup();
    static void InitDefaultKeyBindings();
    void ComputeStaticSceneStats();
    void UpdateSceneUBOAtIndex(u32 frameIndex) const;
    void UpdateSceneUBO();
    void ApplyFreeFlyMovement(u32 idx, f32 dt);
	void UpdateCamera();
    void QueueFrustumVisualizer(u32 camIdx, const glm::vec4& color);
    void CreatePBRSphereGrid();
};
