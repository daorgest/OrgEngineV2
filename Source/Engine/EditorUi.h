//
// Created by Orgest on 7/7/2025.
//

#ifndef EDITORUI_H
#define EDITORUI_H
#include "VulkanCommands.h"
#include "VulkanInit.h"
#include "VulkanRenderPass.h"
#include "Input/InputSys.h"


struct Extent2D;
struct LightUBO;
struct LightMeta;
struct SceneStats;
struct Camera;

namespace Renderer {
	struct FrameContext;
	struct VulkanSwapchain;
	struct ModelComponent;
}

struct EditorUI
{
	VkDescriptorPool imguiDescriptorPool{};

	bool     overlayHidden_ = false;   // global show/hide
	bool     layoutBuilt_   = false;   // build default layout once
	u32		 dockspaceID_   = 0;

	static void InitEditorStyles();
	bool Init(const Renderer::VulkanInstance* instance, const Renderer::VulkanDevice* device, Renderer::VulkanSwapchain* swapchain) const;
	static void Destroy();
	static void BeginFrame();
	static void EndFrame();
	static void Render(VkCommandBuffer cmd);

	// Draws
	void DrawOverlayMinimizeButton();
	static void DrawCameraGizmo(const Camera* camera);
	void DrawCameraProperties(Camera& camera);
	void DrawLightingPanel(Vector<LightUBO>& lights, Camera& camera, Camera*& activeCamera, LightMeta& lightMeta);
	static void HoverToolTip(const char* tooltip);
	void DrawSceneStatsPanel(const Extent2D& extent, const std::string& gpuName, const std::string& driverString, const SceneStats& sceneStats,
	                         const Vector<Renderer::ModelComponent>& models, Camera& camera);

	static void DrawRenderingPanel(Renderer::VulkanSwapchain* swapchain, DebugView& debugData);

	static void DrawCameraSpeedPopup(float cameraSpeed, float& popupTime);
	static void DrawInputDebugUI();
};

#endif //EDITORUI_H
