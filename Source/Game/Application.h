//
// Created by Orgest on 7/13/2025.
//

#pragma once
#include <memory>

#include "Camera.h"
#include "EditorUi.h"
#include "FPSCamera.h"
#include "MeshData.h"
#include "MeshStats.h"
#include "VulkanCommands.h"
#include "VulkanInit.h"
#include "VulkanMesh.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanShader.h"
#include "VulkanShaderBuffer.h"
#include "VulkanSwapchain.h"
#include "../Core/Tools/Arena.h"

constexpr double fixedStepFps = 1.0 / 60.0;

struct Application
{
	void DrawLoadingSplash(const char* text) const;
	bool CreateAabbPipeline(bool depthTest, bool alwaysOnTop);
	inline void QueueBBoxWS(const glm::mat4& model, const glm::vec3& mn, const glm::vec3& mx, const glm::vec4& color, float depthBias,
	                        u32 flags);
	void FlushBBoxWS(VkCommandBuffer cmd, u32 frameIndex);
	bool CreateSkybox();
	bool Init();
	void UpdateCamera();
	void UpdateSceneUBO(const Renderer::FrameContext& frame);
	void RenderScene(const Renderer::FrameContext& frame);
	void ComputeStaticSceneStats();
	void RenderImGui(const Renderer::FrameContext& frame);
	void Run();
	void Cleanup();

	// Core components
	ArenaAllocator coreArena{Megabyte};
	ArenaAllocator renderArena{Megabyte};

	Platform::WindowContext* wc = nullptr;
	Renderer::VulkanInstance* instance = nullptr;
	Renderer::VulkanDevice* device = nullptr;
	Renderer::VulkanSwapchain* swapchain = nullptr;
	Renderer::VulkanRenderer* renderer = nullptr;
	Renderer::VulkanRenderPass* renderPass = nullptr;
	Renderer::DescriptorAllocatorGrowable* globalDescriptorAlloc = nullptr;
	EditorUI* editorUI = nullptr;

	// bounding box debug
	std::unique_ptr<Renderer::VulkanShaderBuffer> aabbSB;
	Renderer::VulkanShader* aabbShader = nullptr;
	Renderer::VulkanPipeline aabbPipeline;
	glm::vec4 aabbColor = {1,1,0,1};
	float     aabbBias  = 0.0f;
	u32		  aabbFlags = 0;
	bool drawAABBs = false;
	bool showGPUInfo = true;
	Vector<BBoxPush> bboxQueue;

	// skybox
	Renderer::VulkanImage skyCubeMap;
	Renderer::VulkanSampler skySampler;
	std::unique_ptr<Renderer::VulkanModel> skyModel;
	Renderer::VulkanShader* skyShader = nullptr;
	Renderer::VulkanPipeline skyPipeline;

	std::unique_ptr<Renderer::VulkanModel> modelInst;
	std::unique_ptr<Renderer::VulkanModel> cubeMesh;

	Renderer::VulkanShaderBuffer* sceneUBO = nullptr;
	Renderer::VulkanShader* shader = nullptr;
	Vector<Renderer::ModelComponent> models;


	// defaults, app will hold the generated fallback images
	Renderer::VulkanImage checkerboardImage;
	Renderer::VulkanSampler* checkerboardSampler = nullptr;
	Renderer::VulkanImage normalFallbackImage;

	SceneStats sceneStats;
	Camera camera;
	FPSCamera fpsCamera;
	Camera* activeCamera = nullptr;
	CameraMode camMode = CameraMode::FreeFly;
	SceneUBO sceneData;
	Vector<LightUBO> lights;
	DebugUBO debugData;
	CameraUBO camUBO;
	LightMeta lightMeta;

	Renderer::VulkanPipeline scenePipeline;

	float aspectRatio;
	f32 cameraSpeed = 5;
	f32 cameraSpeedPopupTime = 0.0;
};
