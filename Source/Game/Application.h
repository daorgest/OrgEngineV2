//
// Created by Orgest on 7/13/2025.
//

#pragma once

#include "Camera.h"
#include "DebugRenderer.h"
#include "FPSCamera.h"
#include "SceneRenderer.h"
#include "ShaderParams.h"
#include "SkyboxManager.h"
#include "VulkanCommands.h"
#include "VulkanInit.h"
#include "VulkanMesh.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanShader.h"
#include "VulkanShaderBuffer.h"
#include "VulkanSwapchain.h"
#include "../Core/Tools/Arena.h"
#include "../Engine/MeshStats.h"

// Forward declaration to break circular dependency with EditorUi.h
struct EditorUI;


// I had fun with arenas but long term im going to remove it as I realized....its annoying
struct Application
{
	void DrawLoadingSplash(const char* text) const;
	void CreatePBRSphereGrid();
	bool Init();
	void UpdateCamera();
	void UpdateSceneUBO(const Renderer::FrameContext& frame);
	void RenderScene(const Renderer::FrameContext& frame);
	void ComputeStaticSceneStats();
	void RenderImGui(const Renderer::FrameContext& frame);
	void Run();
	void Cleanup() const;

	// Core components
	ArenaAllocator coreArena{Megabyte};    // Entire app lifetime
	ArenaAllocator renderArena{Megabyte};  // Per-scene (resettable)(ew)(idk why im so scared of smart pointers)

	Platform::WindowContext* windowContext = nullptr;
	Renderer::VulkanInstance* instance = nullptr;
	Renderer::VulkanDevice* device = nullptr;
	Renderer::VulkanSwapchain* swapchain = nullptr;
	Renderer::VulkanRenderer* renderer = nullptr;
	Renderer::VulkanRenderPass* renderPass = nullptr;
	Renderer::DescriptorAllocatorGrowable* globalDescriptorAlloc = nullptr;
	EditorUI* editorUI = nullptr;

	Renderer::VulkanPipeline scenePipeline;

	// Rendering subsystems
	SkyboxManager skybox;
	DebugRenderer debugRenderer;
	SceneRenderer sceneRenderer;

	// Editor mode state
	bool showGPUInfo = true;
	bool showMenuBar = true;
	bool showLightMenu = true;
	bool shaderHotReloadEnabled = true;
	bool showPerformanceGraphs = true;


	// model and cube
	Renderer::VulkanModel* modelInst = nullptr;      // renderArena
	Renderer::VulkanModel* cubeMesh = nullptr;       // renderArena
	Renderer::VulkanModel* sphereMesh = nullptr;     // renderArena for PBR showcase

	Renderer::VulkanShaderBuffer* sceneUBO = nullptr;
	Renderer::VulkanShader sceneShader;
	Vector<Renderer::ModelComponent> models;


	// defaults
	Renderer::VulkanImage whiteImage;  // Pure white texture for PBR spheres
	Renderer::VulkanImage checkerboardImage;
	Renderer::VulkanSampler* checkerboardSampler = nullptr;
	Renderer::VulkanImage normalFallbackImage;
	Renderer::VulkanSampler* normalFallbackSampler = nullptr;

	SceneStats sceneStats;
	Camera camera;
	FPSCamera fpsCamera;
	Camera* activeCamera = nullptr;
	CameraMode camMode = CameraMode::FreeFly;
	SceneUBO sceneData;
	Vector<LightUBO> lights;
	DebugUBO debugData;
	CameraUBO camUBO;
	LightUBOCount lightMeta;

	float aspectRatio;
	f32 cameraSpeed = 5;
	f32 cameraSpeedPopupTime = 0.0;
	f32 lastFrameMs = 0.0f;    // CPU frame time of the last completed frame
};
