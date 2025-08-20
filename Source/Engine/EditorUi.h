//
// Created by Orgest on 7/7/2025.
//

#ifndef EDITORUI_H
#define EDITORUI_H
#include "imgui.h"
#include "VulkanCommands.h"
#include "VulkanInit.h"
#include "VulkanRenderPass.h"
#include "VulkanTexture.h"


struct Camera;
struct EditorUI
{
	VkDescriptorPool imguiDescriptorPool{};

	static void InitEditorStyles();
	bool Init(const Renderer::VulkanInstance* instance, const Renderer::VulkanDevice* device, Renderer::VulkanSwapchain* swapchain);
	void Destroy();
	void BeginFrame();
	void EndFrame();
	void Render(VkCommandBuffer cmd) const;

	// Draws
	void DrawCameraGizmo(const Camera* camera);
};



#endif //EDITORUI_H
