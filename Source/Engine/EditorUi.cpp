//
// Created by Orgest on 7/7/2025.
//

#include "EditorUi.h"

#include "Camera.h"
#include "imoguizmo.hpp"
#include "MathFuncs.h"
#include "MeshData.h"
#include "MeshStats.h"
#include "RendererTypes.h"
#include "VulkanConvert.h"
#include "VulkanMesh.h"
#include "VulkanRenderPass.h"
#include "VulkanSwapchain.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"
#include "Input/InputSys.h"
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

// For multi-viewport support (we're not using this yet too complicated)
static int CreateVulkanSurfaceForImGui(ImGuiViewport* vp, ImU64 vkInst, const void* vkAlloc, ImU64* outVkSurface)
{
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = (HWND)vp->PlatformHandleRaw;
	createInfo.hinstance = ::GetModuleHandle(nullptr);
	return vkCreateWin32SurfaceKHR((VkInstance)vkInst, &createInfo, (VkAllocationCallbacks*)vkAlloc, (VkSurfaceKHR*)outVkSurface);
}


bool EditorUI::Init(const Renderer::VulkanInstance* instance, const Renderer::VulkanDevice* device,
                    Renderer::VulkanSwapchain* swapchain) const
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
	io.ConfigDpiScaleFonts = true;
	io.ConfigDebugHighlightIdConflicts = true;

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
		.DescriptorPoolSize = 1000,
		.MinImageCount = MAX_FRAME_OVERLAP, // Must match frames in flight, not swapchain images
		.ImageCount = MAX_FRAME_OVERLAP,    // Must match frames in flight, not swapchain images
		.UseDynamicRendering = true,
	};

	ImGui_ImplVulkan_PipelineInfo pipelineInfo = {
		.MSAASamples = Renderer::ToVk(SampleCount::X1),
		.PipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchain->surfaceFormat.format
		},
	};

	ImGui_ImplVulkan_Init(&initInfo);
	ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);

	// Font loading
	ImFontConfig cfg;
	cfg.OversampleH = 1; // less memory, crisper big fonts
	cfg.OversampleV = 1;
	cfg.PixelSnapH = true; // pixel-perfect horizontal
	cfg.RasterizerMultiply = 1.1f; // brighten

	// Loading font
	constexpr f32 fontSize = 18.0f;
	// we're going to rip this out from windows
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
	ImGui::UpdatePlatformWindows();
}

void EditorUI::Render(const VkCommandBuffer cmd)
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}


void EditorUI::DrawOverlayMinimizeButton()
{
	// Tiny button in the top-right corner of the viewport
	ImGuiViewport* vp = ImGui::GetMainViewport();
	const float pad = 8.0f;
	ImVec2 pos = ImVec2(vp->WorkPos.x + vp->WorkSize.x - 28.0f, vp->WorkPos.y + pad);
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.25f);
	ImGuiWindowFlags wflags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove;
	if (ImGui::Begin("##overlay_toggle", nullptr, wflags))
	{
		const char8_t* label = overlayHidden_ ? u8"▶" : u8"◀"; // simple chevron
		if (ImGui::Button(reinterpret_cast<const char*>(label))) overlayHidden_ = !overlayHidden_;
		ImGui::End();
	}
}

// void EditorUI::BuildDefaultLayout()
// {
// 	// Clear any existing layout on this dockspace
// 	ImGui::DockBuilderRemoveNode(dockspaceID_);
// 	ImGui::DockBuilderAddNode(dockspaceID_, ImGuiDockNodeFlags_DockSpace);
// 	ImGui::DockBuilderSetNodeSize(dockspaceID_, ImGui::GetMainViewport()->WorkSize);
//
// 	// Create one central node to host a tab bar
// 	u32 center = dockspaceID_;
//
// 	// Dock windows as tabs in the same node (order defines initial tab order)
// 	ImGui::DockBuilderDockWindow("Scene Stats",   center);
// 	ImGui::DockBuilderDockWindow("Input Debug",   center);
// 	ImGui::DockBuilderDockWindow("Lighting",      center);
// 	ImGui::DockBuilderDockWindow("Rendering",     center);
//
// 	ImGui::DockBuilderFinish(dockspaceID_);
// }

void EditorUI::DrawCameraGizmo(const Camera* camera)
{
	glm::mat4 view = camera->GetViewMatrix();
	const glm::mat4 proj = glm::perspective(Radians(90.0f), 1.0f, 0.1f, 1000.0f);

	const ImGuiIO& io = ImGui::GetIO();
	const float displayWidth = io.DisplaySize.x;
	const float dpiScale = (io.DisplayFramebufferScale.x > 0.0f) ? io.DisplayFramebufferScale.x : 1.0f;

	// UI position / scale
	constexpr float baseGizmoSize = 125.0f;
	constexpr float baseYOffset = 35.0f;
	constexpr float baseXOffset = 35.0f;

	// Apply DPI scaling to sizes and offsets.
	const float gizmoSize = baseGizmoSize * dpiScale;
	const float yOffset = baseYOffset * dpiScale;
	const float xOffset = baseXOffset * dpiScale;
	ImOGuizmo::BeginFrame();
	ImOGuizmo::SetRect(displayWidth - gizmoSize - xOffset, yOffset, gizmoSize);
	ImOGuizmo::DrawGizmo(glm::value_ptr(view), glm::value_ptr(proj), 5.0f);
}

void EditorUI::DrawCameraProperties(Camera& camera)
{
	ImGui::TextUnformatted("Camera");
	ImGui::Separator();

	// 2-column property table: Label | Control
	if (ImGui::BeginTable("CameraProps", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		auto Row = [](const char* label, auto drawWidget)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);

			ImGui::TableSetColumnIndex(1);
			drawWidget();
		};

		// Position
		Row("Position", [&]
		{
			ImGui::PushID("pos");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("World-space position (XYZ)");
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset")) camera.position = {0.0f, 0.0f, 0.0f};
			ImGui::PopID();
		});

		// Forward (normalized)
		Row("Forward", [&]
		{
			ImGui::PushID("fwd");
			bool changed = ImGui::DragFloat3("##value", glm::value_ptr(camera.forward), 0.01f, -1.0f, 1.0f, "%.3f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("View direction. Will be normalized.");
			// Normalize to keep it a proper direction vector
			if (changed)
			{
				if (glm::length2(camera.forward) > 1e-8f)
					camera.forward = glm::normalize(camera.forward);
				else
					camera.forward = {0, 0, -1};
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset")) camera.forward = {0.0f, 0.0f, -1.0f};
			ImGui::PopID();
		});

		// FOV (deg)
		Row("FOV (deg)", [&]
		{
			ImGui::PushID("fov");
			if (ImGui::SliderFloat("##value", &camera.fov, 10.0f, 120.0f, "%.1f"))
			{
				camera.fov = glm::clamp(camera.fov, 1.0f, 179.0f);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Vertical field-of-view in degrees");
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset")) camera.fov = 60.0f;
			ImGui::PopID();
		});

		// Near/Far
		Row("Near / Far", [&]
		{
			ImGui::PushID("nf");
			float nf[2] = {camera.nearPlane, camera.farPlane};
			if (ImGui::InputFloat2("##value", nf, "%.3f"))
			{
				camera.nearPlane = glm::max(0.001f, nf[0]);
				camera.farPlane = glm::max(camera.nearPlane + 0.001f, nf[1]);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clip planes. Keep near as large as possible for depth precision.");
			ImGui::SameLine();
			if (ImGui::SmallButton("Tighten"))
			{
				// optional helper to keep reasonable defaults
				camera.nearPlane = 0.05f;
				camera.farPlane = 2000.0f;
			}
			ImGui::PopID();
		});

		ImGui::EndTable();
	}
}

void EditorUI::DrawLightingPanel(Vector<LightUBO>& lights, Camera& camera, Camera*& activeCamera, LightMeta& lightMeta)
{
	if (!ImGui::Begin("Lighting"))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Lights: %u", static_cast<u32>(lights.size()));
	ImGui::Separator();

	// Quick add buttons
	if (ImGui::Button("+ Directional"))
	{
		LightUBO l{};
		l.type = static_cast<u32>(LightType::Directional);
		l.direction = glm::normalize(glm::vec3{-0.5f, -1.0f, -0.3f});
		l.color = glm::vec3(1.0f, 0.95f, 0.85f);
		l.intensity = 2.0f;
		lights.push_back(l);
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Point"))
	{
		LightUBO l{};
		l.type = static_cast<u32>(LightType::Point);
		l.position = activeCamera->position + activeCamera->forward * 2.0f;
		l.range = 8.0f;
		l.color = glm::vec3(1.0f);
		l.intensity = 5.0f;
		lights.push_back(l);
	}
	ImGui::SameLine();
	if (ImGui::Button("+ Spot"))
	{
		LightUBO l{};
		l.type = static_cast<u32>(LightType::Spot);
		l.position = activeCamera->position;
		l.direction = glm::normalize(activeCamera->forward);
		l.range = 12.0f;
		l.innerCone = std::cos(glm::radians(12.0f));
		l.outerCone = std::cos(glm::radians(20.0f));
		l.color = glm::vec3(1.0f);
		l.intensity = 6.0f;
		lights.push_back(l);
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) lights.clear();

	ImGui::Separator();

	constexpr float dirStep = 0.05f;
	constexpr float posStep = 0.05f;
	constexpr float intensityStep = 0.25f;
	constexpr float rangeStep = 0.5f;

	static const char* typeLabels[] = {"Directional", "Point", "Spot"};

	auto normalizeIfNonZero = [](glm::vec3 v)
	{
		const float len2 = glm::length2(v);
		return (len2 > 1e-6f) ? glm::normalize(v) : glm::vec3(0.0f, 1.0f, 0.0f);
	};

	for (size_t i = 0; i < lights.size(); /* inc inside */)
	{
		ImGui::PushID(static_cast<int>(i));
		ImGui::SeparatorText(("Light " + std::to_string(i)).c_str());

		LightUBO& l = lights[i];

		int type = static_cast<int>(l.type);
		if (ImGui::Combo("Type", &type, typeLabels, IM_ARRAYSIZE(typeLabels)))
			l.type = static_cast<u32>(type);

		ImGui::ColorEdit3("Color", &l.color.x);
		ImGui::DragFloat("Intensity", &l.intensity, intensityStep, 0.0f, 1000.0f);

		if (l.type == static_cast<u32>(LightType::Directional))
		{
			if (ImGui::DragFloat3("Direction", &l.direction.x, dirStep, -1.0f, 1.0f))
				l.direction = normalizeIfNonZero(l.direction);

			if (ImGui::Button("Face Camera"))
				l.direction = normalizeIfNonZero(camera.forward);
		}
		else if (l.type == static_cast<u32>(LightType::Point))
		{
			ImGui::DragFloat3("Position", &l.position.x, posStep);
			ImGui::DragFloat("Range", &l.range, rangeStep, 0.0f, 1000.0f);
		}
		else // Spot
		{
			ImGui::DragFloat3("Position", &l.position.x, posStep);

			if (ImGui::DragFloat3("Direction", &l.direction.x, dirStep, -1.0f, 1.0f))
				l.direction = normalizeIfNonZero(l.direction);

			ImGui::SameLine();
			if (ImGui::Button("Snap To Camera"))
			{
				l.position = activeCamera->position;
				l.direction = normalizeIfNonZero(camera.forward);
			}

			ImGui::DragFloat("Range", &l.range, rangeStep, 0.1f, 1000.0f);

			auto clamp1 = [](float v) { return std::clamp(v, -1.0f, 1.0f); };
			float innerRad = std::acos(clamp1(l.innerCone));
			float outerRad = std::acos(clamp1(l.outerCone));

			bool changed = false;
			changed |= ImGui::SliderAngle("Inner", &innerRad, 0.0f, 80.0f, "%.1f°");

			const float minSep = glm::radians(1.0f);
			const float outerMin = innerRad + minSep;
			changed |= ImGui::SliderAngle("Outer", &outerRad, outerMin, 89.0f, "%.1f°");

			if (changed)
			{
				l.innerCone = std::cos(innerRad);
				l.outerCone = std::cos(outerRad);
			}
		}

		bool removed = false;
		if (ImGui::Button("Remove"))
		{
			// lights.erase(lights.begin() + static_cast<ptrdiff_t>(i));
			removed = true;
		}

		ImGui::PopID();
		if (!removed) ++i;
	}

	// keep meta in sync
	lightMeta.count = static_cast<u32>(lights.size());

	ImGui::End();
}

void EditorUI::HoverToolTip(const char* tooltip)
{
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(tooltip);
		ImGui::EndTooltip();
	}
}

void EditorUI::DrawSceneStatsPanel(const Extent2D& extent, const std::string& gpuName, const std::string& driverString, const SceneStats& sceneStats,
                                   const Vector<Renderer::ModelComponent>& models, Camera& camera)
{
	if (!ImGui::Begin("Scene Stats"))
	{
		ImGui::End();
		return;
	}

	// Overview
	if (ImGui::BeginTable("Overview", 2, ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted("Resolution");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%ux%u", extent.width, extent.height);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted("FPS");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted("GPU");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s", gpuName.c_str());

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted("Driver Ver");
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%s", driverString.c_str());
		ImGui::EndTable();
	}

	ImGui::Separator();

	// Camera properties
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		DrawCameraProperties(camera);

	ImGui::Separator();

	// Scene stats table (recompute verts/tris like before)
	u32 totalVerts = 0;
	u32 totalTris = 0;
	u32 totalDraws = 0;
	for (const auto& [model, transform] : models)
	{
		if (model == nullptr) continue;
		for (const auto& part : model->parts)
		{
			if (part.vertexBuffer.buffer != VK_NULL_HANDLE)
				totalVerts += static_cast<u32>(part.vertexBuffer.allocationInfo.size / sizeof(Vertex));
			totalTris += part.indexCount / 3;
			++totalDraws;
		}
	}

	if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("SceneTable", 2, ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Draw Calls");
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%u", sceneStats.drawCallCount);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Vertices");
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%u", totalVerts);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Triangles");
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%u", totalTris);
			ImGui::EndTable();
		}
	}

	ImGui::Separator();

	// Timing
	const double gpuBusy = std::clamp((sceneStats.gpuDrawTime / (1000.0 / ImGui::GetIO().Framerate)) * 100.0, 0.0, 100.0);
	if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("TimingTable", 2, ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("CPU Draw Time");
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.3f ms", sceneStats.cpuDrawTime);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("GPU Draw Time");
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.3f ms", sceneStats.gpuDrawTime);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("GPU Busy");
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.1f %%", gpuBusy);
			ImGui::EndTable();
		}
	}

	ImGui::End();
}

void EditorUI::DrawRenderingPanel(Renderer::VulkanSwapchain* swapchain, DebugView& debugData)
{
	if (!ImGui::Begin("Rendering"))
	{
		ImGui::End();
		return;
	}

	// Debug view mode
	if (ImGui::BeginCombo("Debug View", DebugViewToString(debugData)))
	{
		for (const auto& [value, label] : kDebugViews)
		{
			bool selected = (debugData == value);
			if (ImGui::Selectable(label, selected)) debugData = value;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// VSync mode
	static const char* vsyncModes[] = {"VSync On", "VSync Off", "Mailbox"};
	int currentVsync = static_cast<int>(swapchain->presentMode);
	if (ImGui::Combo("VSync Mode", &currentVsync, vsyncModes, IM_ARRAYSIZE(vsyncModes)))
		swapchain->VsyncEnable(static_cast<PresentMode>(currentVsync));

	ImGui::End();
}

void EditorUI::DrawCameraSpeedPopup(float cameraSpeed, float& popupTime)
{
	if (popupTime <= 0.0f) return;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 pos = ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.12f);

	const float alpha = std::min(1.0f, popupTime / 1.0f);
	ImGui::SetNextWindowBgAlpha(alpha);
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoInputs;

	if (ImGui::Begin("##CameraSpeedPopup", nullptr, flags))
		ImGui::Text("Speed: %.2f", cameraSpeed);
	ImGui::End();
	ImGui::PopStyleVar();

	popupTime -= ImGui::GetIO().DeltaTime;
	ImGui::End();
}

// tiny helper for colored on/off badges
static void Badge(const char* label, bool on, ImU32 onCol, ImU32 offCol)
{
	ImGui::SameLine(0, 0);
	ImGui::PushStyleColor(ImGuiCol_Button, on ? onCol : offCol);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? onCol : offCol);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, on ? onCol : offCol);
	ImGui::Button(label);
	ImGui::PopStyleColor(3);
}

// Optional: names for a few keyboard keys you care about listing quickly.
// If you want all of them, generate a table from your enum → string.
static const char* KeyName(int k)
{
	using K = Keyboard::Key;
	switch (static_cast<K>(k))
	{
	case K::W: return "W";
	case K::A: return "A";
	case K::S: return "S";
	case K::D: return "D";
	case K::Q: return "Q";
	case K::E: return "E";
	case K::Space: return "Space";
	case K::Shift: return "Shift";
	case K::Ctrl: return "Ctrl";
	case K::Alt: return "Alt";
	case K::F1: return "F1";
	case K::F2: return "F2";
	case K::F11: return "F11";
	case K::Escape: return "Esc";
	default: return nullptr;
	}
}

// Call this from your debug menu
void EditorUI::DrawInputDebugUI()
{
	if (!ImGui::Begin("Input Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	// Summary row
	{
		ImGui::Text("Active Source:");
		Badge("Keyboard", input.usingKeyboard, IM_COL32(88, 180, 88, 255), IM_COL32(60, 60, 60, 255));
		Badge("Mouse", input.usingMouse, IM_COL32(88, 180, 255, 255), IM_COL32(60, 60, 60, 255));
		Badge("Gamepad", input.usingController, IM_COL32(255, 180, 88, 255), IM_COL32(60, 60, 60, 255));
		ImGui::NewLine();
	}

	// Mouse
	if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Delta: (%.1f, %.1f)   Wheel: (%d, %d)", input.xrel, input.yrel, (int)input.scrollX, (int)input.scrollY);

		ImGui::SeparatorText("Buttons");
		ImGui::BeginTable("mouseBtns", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);
		ImGui::TableSetupColumn("Btn");
		ImGui::TableSetupColumn("Held");
		ImGui::TableSetupColumn("Pressed");
		ImGui::TableSetupColumn("Released");
		ImGui::TableHeadersRow();

		auto rowBtn = [&](const char* name, int idx)
		{
			const ButtonState& b = input.mouseButtons[idx];
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(name);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%s", b.held ? "true" : "false");
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%s", b.pressed ? "true" : "false");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%s", b.released ? "true" : "false");
		};
		rowBtn("Left", Mouse::Left);
		rowBtn("Right", Mouse::Right);
		rowBtn("Middle", Mouse::Middle);
		rowBtn("X1", Mouse::Button4);
		rowBtn("X2", Mouse::Button5);

		ImGui::EndTable();
	}

	// Keyboard
	if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Show commonly interesting keys in a compact grid
		ImGui::SeparatorText("Common keys");
		ImGui::BeginTable("kbGrid", 6, ImGuiTableFlags_SizingFixedFit);
		int shown = 0;
		for (int k = 0; k < (int)Keyboard::ButtonCount; ++k)
		{
			if (const char* nm = KeyName(k))
			{
				if ((shown++ % 6) == 0) ImGui::TableNextRow();
				ImGui::TableSetColumnIndex((shown - 1) % 6);
				const ButtonState& b = input.keyboard[k];
				ImU32 col = b.held ? IM_COL32(120, 200, 120, 255) : IM_COL32(80, 80, 80, 255);
				ImGui::PushStyleColor(ImGuiCol_Button, col);
				ImGui::Button(nm, ImVec2(44, 0));
				ImGui::PopStyleColor();
				if (b.pressed)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.7f, 1, 0.7f, 1), "*");
				}
				if (b.released)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1, 0.6f, 0.6f, 1), "v");
				}
			}
		}
		ImGui::EndTable();

		// Optionally list ALL currently held keys
		if (ImGui::TreeNode("Held keys (all)"))
		{
			ImGui::TextUnformatted("Indices:");
			ImGui::SameLine();
			bool first = true;
			for (int k = 0; k < (int)Keyboard::ButtonCount; ++k)
				if (input.keyboard[k].held)
				{
					if (!first) ImGui::SameLine();
					ImGui::Text("%d", k);
					first = false;
				}
			if (first) ImGui::TextDisabled("none");
			ImGui::TreePop();
		}
	}

	ImGui::End();
}
