//
// Created by Orgest on 7/13/2025.
//

#pragma once
#include "Arena.h"
#include "Camera.h"
#include "EditorUi.h"
#include "MeshData.h"
#include "MeshStats.h"
#include "VulkanCommands.h"
#include "VulkanInit.h"
#include "VulkanMesh.h"
#include "VulkanRenderPass.h"
#include "VulkanShader.h"
#include "VulkanSwapchain.h"
#include "VulkanShaderBuffer.h"

struct Application
{
	bool Init();
	void UpdateCamera();
	void UpdateSceneUBO(const Renderer::FrameContext& frame);
	void RenderScene(const Renderer::FrameContext& frame);
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

	std::unique_ptr<Renderer::VulkanModel> model;
	std::unique_ptr<Renderer::VulkanModel> cubeMesh;
	Renderer::VulkanShaderBuffer* sceneUBO = nullptr;
	Renderer::VulkanShader* shader = nullptr;
	Vector<Renderer::VulkanModelComponent> models;


	// defaults
	Renderer::VulkanImage* checkerboardImage = nullptr;
	Renderer::VulkanSampler* checkerboardSampler = nullptr;

	SceneStats sceneStats;
	Camera camera;
	UBO sceneData;
	Vector<LightUBO> lights;
	DebugUBO debugData;
	CameraUBO camUBO;
	LightMeta lightMeta;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkPipeline debugPipeline = VK_NULL_HANDLE;
	VkPipelineLayout debugPipelineLayout = VK_NULL_HANDLE;

	float aspectRatio;
	f32 cameraSpeed = 5;
	f32 cameraSpeedPopupTime = 0.0;
};
