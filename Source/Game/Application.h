  //
// Created by Orgest on 7/13/2025.
//

#pragma once

#include "BindlessManager.h"
#include "Camera.h"
#include "DebugRenderer.h"
#include "DefaultTextures.h"
#include "EditorUi.h"
#include "Platform.h"
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
#include "../Core/Tools/Arena.h"
#include "../Engine/MeshStats.h"
#include "Audio/Audio.h"

// Forward declaration to break circular dependency with EditorUi.h
struct EditorUI;

// I had fun with arenas but long term im going to remove it as I realized....it's annoying
struct Application
{
	// Core components
	// ArenaAllocator coreArena{Megabyte};    // Entire app lifetime
	// ArenaAllocator renderArena{Megabyte};  // Per-scene (resettable)(ew)(I don't know why im so scared of smart pointers)

    Platform::WindowContext windowContext;

	Renderer::VulkanInstance instance;
	Renderer::VulkanDevice device;
	Renderer::VulkanSwapchain swapchain;
	Renderer::VulkanRenderer renderer;
	Renderer::DescriptorAllocatorGrowable globalDescriptorAlloc;

	SkyboxManager skybox;
	DebugRenderer debugRenderer;
	SceneRenderer sceneRenderer;

    Renderer::ShaderCompiler compiler;
    Renderer::VulkanShaderManager shaderManager;
    Audio::System audioSys;
    EditorUI editorUI;

    AssetPool<TextureData> texturePool;
    AssetPool<Renderer::GPUModel> modelPool;
    Renderer::BindlessManager bindlessManager;

	Vector<Renderer::ModelComponent> models;

    CameraMode camMode = CameraMode::FreeFly;
    Array<CameraComponent, MAX_SCENE_CAMERAS> sceneCameras;
    CameraComponent frozenCamComp;

    u32 activeCamIdx = 0;
    u32 selectedCameraIdx = 0;
    u32 frustumIdx = 0;


    std::unique_ptr<Renderer::VulkanShaderBuffer> sceneUBO;
	SceneUBO sceneData;
	DebugUBO debugData;
	CameraUBO camUBO;
	LightSceneData lightUBO;
    Vector<LightUBO> lights;
	SceneStats sceneStats;

	f32 aspectRatio;
	f32 cameraSpeed = 5;
	f32 cameraSpeedPopupTime = 0.0f;
    f32 debugViewPopupTime = 0.0f;
	f32 lastFrameMs = 0.0f;
	bool freezeFrustum = false;

	bool Init();

    void Run();
    void RenderLoadingSplash(const char* text);
    void RenderScene(u32 imageIndex);
    void RenderImGui(u32 imageIndex);
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
