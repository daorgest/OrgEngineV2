  //
// Created by Orgest on 7/13/2025.
//

#pragma once

#include "Camera.h"
#include "DebugRenderer.h"
#include "DefaultTextures.h"
#include "EditorUi.h"
#include "Platform.h"
#include "SceneRenderer.h"
#include "ShaderParams.h"
#include "SkyboxManager.h"
#include "VulkanCommands.h"
#include "VulkanDevice.h"
#include "VulkanInit.h"
#include "VulkanMesh.h"
#include "VulkanPipeline.h"
#include "VulkanShader.h"
#include "VulkanShaderBuffer.h"
#include "VulkanSwapchain.h"
#include "../Core/Tools/Arena.h"
#include "../Engine/MeshStats.h"

// Forward declaration to break circular dependency with EditorUi.h
struct EditorUI;

// I had fun with arenas but long term im going to remove it as I realized....it's annoying
struct Application
{
	// Core components
	ArenaAllocator coreArena{Megabyte};    // Entire app lifetime
	ArenaAllocator renderArena{Megabyte};  // Per-scene (resettable)(ew)(I don't know why im so scared of smart pointers)

	Renderer::VulkanInstance instance;
	Renderer::VulkanDevice device;
	Renderer::VulkanSwapchain swapchain;
	Renderer::VulkanRenderer renderer;
	Renderer::DescriptorAllocatorGrowable globalDescriptorAlloc;

	SkyboxManager skybox;
	DebugRenderer debugRenderer;
	SceneRenderer sceneRenderer;

	Renderer::VulkanPipeline scenePipeline;
	Renderer::VulkanShader sceneShader;

	std::unique_ptr<Renderer::VulkanShaderBuffer> sceneUBO;

	Renderer::TextureDefaults texDefaults;

	std::unique_ptr<Renderer::GPUSampler> checkerboardSampler;
	std::unique_ptr<Renderer::GPUSampler> normalFallbackSampler;

	// model and cube
	std::unique_ptr<Renderer::VulkanModel> modelInst;
	// std::unique_ptr<Renderer::VulkanModel> cubeMesh; // not used
	std::unique_ptr<Renderer::VulkanModel> sphereMesh;
	Vector<Renderer::ModelComponent> models;
	Vector<LightUBO> lights;

	Platform::WindowContext windowContext;
	EditorUI editorUI;

    CameraMode camMode = CameraMode::FreeFly;
    Array<CameraComponent, MAX_SCENE_CAMERAS> sceneCameras;
    CameraComponent frozenCamComp;

    u32 activeCamIdx = 0;
    u32 selectedCameraIdx = 0;
    u32 frustumIdx = 0;
    bool freezeFrustum = false;

	// Plain Old Data (POD) structs
	SceneUBO sceneData;
	DebugUBO debugData;
	CameraUBO camUBO;
	LightUBOCount lightMeta;
	SceneStats sceneStats;

	f32 aspectRatio;
	f32 cameraSpeed = 5;
	f32 cameraSpeedPopupTime = 0.0;
	f32 lastFrameMs = 0.0f;
	bool showGPUInfo = true;
	bool showMenuBar = true;
	bool showEditorTools = true;

	bool Init();
    static void InitDefaultBindings();
    void Run();
	void Cleanup();
	void UpdateCamera();
    void QueueFrustumVisualizer(u32 camIdx, const glm::vec4& color);
    void ApplyFreeFlyMovement(u32 idx, f32 dt);
    void UpdateSceneUBO();
	void RenderScene(u32 imageIndex);
	void RenderImGui(u32 imageIndex);
	void DrawLoadingSplash(const char* text);
	void CreatePBRSphereGrid();
	void ComputeStaticSceneStats();
};
