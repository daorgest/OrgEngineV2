//
// Created by Orgest on 7/7/2025.
//

#include "EditorUi.h"

#include "Camera.h"
#include "imoguizmo.hpp"
#include "Mat4x4.h"
#include "MathFuncs.h"
#include "VulkanRenderPass.h"
#include "VulkanSwapchain.h"
#ifndef IMGUI_DISABLE
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <backends/imgui_impl_vulkan.h>
#endif

void EditorUI::InitEditorStyles()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.35f, 0.63f, 0.78f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.13f, 0.94f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.19f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.00f, 0.32f, 0.39f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.38f, 0.44f, 0.66f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.00f, 0.36f, 0.33f, 0.36f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.36f, 0.37f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.63f, 0.36f, 0.78f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.64f, 0.69f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.00f, 0.60f, 0.55f, 0.42f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.83f, 0.79f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.69f, 0.82f, 0.96f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.36f, 0.33f, 0.46f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.71f, 0.69f, 0.54f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.43f, 0.50f, 0.59f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.74f, 0.26f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.55f, 0.26f, 0.98f, 0.95f);
	colors[ImGuiCol_InputTextCursor] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.71f, 0.69f, 0.77f);
	colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.13f, 0.85f);
	colors[ImGuiCol_TabSelected] = ImVec4(0.00f, 0.71f, 0.69f, 0.56f);
	colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.28f, 0.69f, 0.91f, 1.00f);
	colors[ImGuiCol_TabDimmed] = ImVec4(0.23f, 0.23f, 0.23f, 0.62f);
	colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
	colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.67f, 0.71f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_TreeLines] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);


	// Styles
	style.WindowPadding = ImVec2(12, 10);
	style.FramePadding = ImVec2(12, 4);
	style.ItemSpacing = ImVec2(8, 4);
	style.ItemInnerSpacing = ImVec2(4, 4);
	style.TouchExtraPadding = ImVec2(0, 0);
	style.IndentSpacing = 17.0f;
	style.ScrollbarSize = 15.0f;
	style.GrabMinSize = 12.0f;
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowRounding = 8.0f;
	style.ChildRounding = 3.0f;
	style.FrameRounding = 5.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabRounding = 3.0f;
	style.TabBorderSize = 1.0f;
	style.TabBarBorderSize = 2.0f;
	style.TabRounding = 5.0f;
	style.CellPadding = ImVec2(6, 4);
	style.TableAngledHeadersAngle = 22.0f;
	style.TableAngledHeadersTextAlign = ImVec2(0.50f, 0.00f);
	style.WindowTitleAlign = ImVec2(0.00f, 0.50f);
	style.WindowBorderHoverPadding = 3.0f;
	style.WindowMenuButtonPosition = ImGuiDir_Right;
	style.ColorButtonPosition = ImGuiDir_Left;
	style.SelectableTextAlign = ImVec2(0.00f, 0.50f);
	style.SeparatorTextAlign = ImVec2(0.50f, 0.50f);
	style.SeparatorTextPadding = ImVec2(20.0f, 5.0f);
	style.LogSliderDeadzone = 4.0f;
	style.ImageBorderSize = 1.0f;
}

// For multi-viewport support (we not using this yet too complicated)
static int CreateVulkanSurfaceForImGui(ImGuiViewport* vp, ImU64 vkInst, const void* vkAlloc, ImU64* outVkSurface)
{
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = (HWND)vp->PlatformHandleRaw;
	createInfo.hinstance = ::GetModuleHandle(nullptr);
	return vkCreateWin32SurfaceKHR((VkInstance)vkInst, &createInfo, (VkAllocationCallbacks*)vkAlloc, (VkSurfaceKHR*)outVkSurface);
}


bool EditorUI::Init(const Renderer::VulkanInstance* instance, const Renderer::VulkanDevice* device,
                                  Renderer::VulkanSwapchain* swapchain)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

	io.ConfigDpiScaleFonts = true;
	ImGui::GetWindowDpiScale();

	InitEditorStyles();

	ImGui_ImplWin32_Init(swapchain->handle);

	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Platform_CreateVkSurface = CreateVulkanSurfaceForImGui;

	ImGui_ImplVulkan_InitInfo initInfo = {
		.ApiVersion = instance->appInfo.apiVersion,
		.Instance = instance->instance,
		.PhysicalDevice = device->physicalDevice,
		.Device = device->device,
		.QueueFamily = device->graphicsQueueIndex,
		.Queue = device->graphicsQueue,
		.DescriptorPool = imguiDescriptorPool,
		.MinImageCount = swapchain->imageCount,
		.ImageCount = swapchain->imageCount,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.DescriptorPoolSize = 1000, // yay no bloat thx zeux
		.UseDynamicRendering = true,
		.PipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchain->surfaceFormat.format
		}
	};

	ImGui_ImplVulkan_Init(&initInfo);

	// Font loading
	ImFontConfig cfg;
	cfg.OversampleH = 1; // less memory, crisper big fonts
	cfg.OversampleV = 1;
	cfg.PixelSnapH = true; // pixel-perfect horizontal
	cfg.RasterizerMultiply = 1.1f; // brighten

	// Loading font
	constexpr f32 fontSize = 18.0f;
	// we gonna rip this out from windows
	io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\segoeui.ttf)", fontSize, &cfg);
	return true;
}

void EditorUI::Destroy()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void EditorUI::BeginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void EditorUI::EndFrame()
{
	ImGui::Render();
}

void EditorUI::Render(VkCommandBuffer cmd) const
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void EditorUI::DrawCameraGizmo(const Camera* camera)
{
	Mat4x4 viewMatrix = camera->GetViewMatrix();
	const Mat4x4 gizmoProj = Mat4x4::Perspective(Radians(90.0f), 1.0f, 0.1f, 1000.0f);

	const ImGuiIO& io = ImGui::GetIO();
	const float displayWidth = io.DisplaySize.x;

	const float dpiScale = 1;

	// UI position / scale
	constexpr float baseGizmoSize = 125.0f;
	constexpr float baseYOffset = 35.0f;
	constexpr float baseXOffset = 35.0f;

	// Apply DPI scaling to sizes and offsets.
	const float gizmoSize = baseGizmoSize * dpiScale;
	const float yOffset = baseYOffset * dpiScale;
	const float xOffset = baseXOffset * dpiScale;
	ImOGuizmo::SetRect(displayWidth - gizmoSize - xOffset, yOffset, gizmoSize);
	ImOGuizmo::BeginFrame();
	ImOGuizmo::DrawGizmo(viewMatrix.m, gizmoProj.m, 5.0f);
}
