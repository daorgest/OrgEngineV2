//
// Created by Orgest on 7/7/2025.
//

#include "EditorUi.h"

#include "Application.h"
#include "Camera.h"
#include "DebugRenderer.h"
#include "imoguizmo.hpp"
#include "MathFuncs.h"
#include "MeshStats.h"
#include "RendererTypes.h"
#include "VulkanConvert.h"
#include "VulkanSwapchain.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"
#include "Input/InputSys.h"
#ifndef IMGUI_DISABLE
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#if ENGINE_PLATFORM_WIN32
#include <backends/imgui_impl_win32.h>
#elif ENGINE_PLATFORM_SDL
#include <backends/imgui_impl_sdl3.h>
#include "SDL3/SDL_vulkan.h"
#endif

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

// For multi-viewport support
#if ENGINE_PLATFORM_WIN32
static int CreateVulkanSurfaceForImGui(ImGuiViewport* vp, ImU64 vkInst, const void* vkAlloc, ImU64* outVkSurface)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = (HWND)vp->PlatformHandleRaw;
    createInfo.hinstance = ::GetModuleHandle(nullptr);

    return vkCreateWin32SurfaceKHR(
        (VkInstance)vkInst,
        &createInfo,
        (const VkAllocationCallbacks*)vkAlloc,
        (VkSurfaceKHR*)outVkSurface
    );
}

#elif ENGINE_PLATFORM_SDL

static int CreateVulkanSurfaceForImGui(ImGuiViewport* vp, ImU64 vkInst, const void* vkAlloc, ImU64* outVkSurface)
{
    auto* win = static_cast<SDL_Window*>(vp->PlatformHandleRaw);

    if (!SDL_Vulkan_CreateSurface(win, (VkInstance)vkInst, nullptr, (VkSurfaceKHR*)outVkSurface))
        return -1;

    return 0;
}

#endif

bool EditorUI::Init(const Renderer::VulkanInstance* instance, const Renderer::VulkanDevice* device,
                    Renderer::VulkanSwapchain* swapchain)
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

#if ENGINE_PLATFORM_WIN32

    // Win32 supports multi-viewports fully
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplWin32_Init(swapchain->handle);

    // Register Vulkan surface creator for platform windows
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateVkSurface = CreateVulkanSurfaceForImGui;

#elif ENGINE_PLATFORM_SDL

    // SDL backend does NOT implement platform windows -> must disable viewports
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    SDL_Window* sdlWin = static_cast<SDL_Window*>(swapchain->handle);
    ImGui_ImplSDL3_InitForVulkan(sdlWin);
#endif

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    ImGui_ImplVulkan_InitInfo initInfo = {
        .ApiVersion = instance->appInfo.apiVersion,
        .Instance = instance->instance,
        .PhysicalDevice = device->physicalDevice,
        .Device = device->device,
        .QueueFamily = device->graphicsQueueIndex,
        .Queue = device->graphicsQueue,
        .DescriptorPool = descriptorPool,
        .DescriptorPoolSize = 1000,
        .MinImageCount = MAX_FRAME_OVERLAP, // Must match frames in flight, not swapchain images
        .ImageCount = MAX_FRAME_OVERLAP, // Must match frames in flight, not swapchain images
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
#if ENGINE_PLATFORM_WIN32
    ImGui_ImplWin32_Shutdown();
#elif ENGINE_PLATFORM_SDL
    ImGui_ImplSDL3_Shutdown();
#endif
    ImGui::DestroyContext();
}

void EditorUI::BeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
#if ENGINE_PLATFORM_WIN32
    ImGui_ImplWin32_NewFrame();
#elif ENGINE_PLATFORM_SDL
    ImGui_ImplSDL3_NewFrame();
#endif
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

void EditorUI::DrawCameraGizmo(const Camera* camera)
{
    glm::mat4 view = camera->GetViewMatrix();
    const glm::mat4 proj = glm::perspective(Radians(90.0f), 1.0f, 0.1f, 1000.0f);

    // Use viewport work area like stats overlay (accounts for menu bar and docking)
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    // Simple hardcoded sizes
    constexpr float gizmoSize = 125.0f;
    constexpr float padding = 10.0f;

    // Position in top-right corner of work area
    const float xPos = work_pos.x + work_size.x - gizmoSize - padding;
    const float yPos = work_pos.y + padding;

    // Draw directly using viewport foreground draw list (sticks to main viewport)
    ImOGuizmo::SetRect(xPos, yPos, gizmoSize);
    ImOGuizmo::SetDrawList(ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(viewport)));
    ImOGuizmo::BeginFrame();
    ImOGuizmo::DrawGizmo(glm::value_ptr(view), glm::value_ptr(proj), 5.0f);
}

void EditorUI::DrawCameraProperties(Camera& camera)
{
    ImGui::TextUnformatted("Camera");
    ImGui::Separator();

    // 2-column property table: Label | Control
    if (ImGui::BeginTable("CameraProps", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
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
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Clip planes. Keep near as large as possible for depth precision.");
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

void EditorUI::HoverToolTip(const char* tooltip)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tooltip);
        ImGui::EndTooltip();
    }
}

void EditorUI::DrawCameraSpeedPopup(f32 camSpeedPopupTime)
{
    if (camSpeedPopupTime <= 0.0f) return;

    // Use viewport work area to position relative to usable screen space
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    // Position in top-center of work area (12% down from top)
    const ImVec2 pos = {work_pos.x + (work_size.x * 0.5f), work_pos.y + (work_size.y * 0.12f)};

    const float alpha = std::min(1.0f, camSpeedPopupTime / 1.0f);

    ImGui::SetNextWindowBgAlpha(alpha);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##CameraSpeedPopup", nullptr, flags))
    {
        ImGui::Text("Speed: %.2f", state.cameraSpeed);
    }


    ImGui::PopStyleVar();

    state.cameraSpeed -= ImGui::GetIO().DeltaTime;
    ImGui::End();
}


bool EditorUI::DrawMainMenuBar()
{
    bool shouldExit = false;

    if (!state.showMenuBar)
        return shouldExit;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
            {
                shouldExit = true;
            }
            HoverToolTip("Close this application?");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Show Menu Bar", "F3", &state.showMenuBar);
            ImGui::MenuItem("Show GPU Info", "F4", &state.showGPUInfo);
            // ImGui::MenuItem("Shader Hot Reload", nullptr, &state.shaderHotReloadEnabled);
            // EditorUI::HoverToolTip("Automatically recompile and reload shaders when .slang files change");

            if (ImGui::BeginMenu("Render Views"))
            {
                for (const auto& [value, label] : kDebugViews)
                {
                    bool selected = (state.debugData->debugMode == value);
                    if (ImGui::MenuItem(label, nullptr, selected)) state.debugData->debugMode = value;
                    if (selected) ImGui::SetItemDefaultFocus();
                }

                EditorUI::HoverToolTip("Debug Views");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("VSync"))
            {
                for (const auto& [mode, label] : kVsyncModes)
                {
                    const bool selected = (state.swapchain->presentMode == mode);
                    if (ImGui::MenuItem(label, nullptr, selected))
                    {
                        state.swapchain->VsyncEnable(mode);
                    }
                    if (selected) { ImGui::SetItemDefaultFocus(); }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    return shouldExit;
}

void EditorUI::DrawMainOverlay()
{
    using namespace ImGui;

    const bool showDepthRange =
        state.debugData->debugMode == DebugView::DepthLin ||
        state.debugData->debugMode == DebugView::DepthLog;

    enum Corner { Custom = -1, TopLeft, TopRight, BottomLeft, BottomRight, Center };
    static int corner = TopLeft;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 work_pos = vp->WorkPos;
    const ImVec2 work_size = vp->WorkSize;

    // Scale pad with DPI/work area; clamp to reasonable range.
    const float pad = std::clamp(10.0f * vp->DpiScale, 8.0f, 24.0f);

    ImVec2 window_pos;
    ImVec2 window_pivot;

    if (corner == TopLeft)
    {
        window_pos = ImVec2(work_pos.x + pad, work_pos.y + pad);
        window_pivot = ImVec2(0.0f, 0.0f);
    }
    else if (corner == TopRight)
    {
        window_pos = ImVec2(work_pos.x + work_size.x - pad, work_pos.y + pad);
        window_pivot = ImVec2(1.0f, 0.0f);
    }
    else if (corner == BottomLeft)
    {
        window_pos = ImVec2(work_pos.x + pad, work_pos.y + work_size.y - pad);
        window_pivot = ImVec2(0.0f, 1.0f);
    }
    else if (corner == BottomRight)
    {
        window_pos = ImVec2(work_pos.x + work_size.x - pad, work_pos.y + work_size.y - pad);
        window_pivot = ImVec2(1.0f, 1.0f);
    }

    if (corner != Custom)
    {
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pivot);
    }

    // Base minimum size
    ImVec2 min_size = ImVec2(220.0f, 100.0f);

    if (showDepthRange)
        min_size.x = std::max(min_size.x, 320.0f);
    if (state.debugRenderer->enabled)
        min_size.x = std::max(min_size.x, 420.0f);

    // Expand height for extra rows
    float extra_rows = 0.0f;
    if (showDepthRange) extra_rows += 1.0f;
    if (state.debugRenderer->enabled) extra_rows += 5.0f;
    min_size.y += extra_rows * ImGui::GetFrameHeightWithSpacing();

    // Max size = screen area minus padding
    const ImVec2 max_size = ImVec2(
        std::max(150.0f, work_size.x - pad * 2.0f),
        std::max(80.0f, work_size.y - pad * 2.0f)
    );

    // Clamp again just to be safe
    min_size.x = std::clamp(min_size.x, 150.0f, max_size.x);
    min_size.y = std::clamp(min_size.y, 80.0f, max_size.y);

    ImGui::SetNextWindowSizeConstraints(min_size, max_size);
    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_HorizontalScrollbar;

    if (ImGui::Begin("Simple overlay", nullptr, flags))
    {
        // Drag to move only when in Custom mode
        if (corner == Custom)
        {
            ImGui::SetNextItemAllowOverlap();
            ImGui::SetWindowCollapsed(false);
        }

        ImGui::SeparatorText("Performance");
        {
            // Aligned rows for timings
            if (ImGui::BeginTable("PerfTable", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("FPS");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f (%.3f ms)", state.wc->fps, state.wc->frameTime);
#ifdef ENABLE_GPU_TIMING
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("GPU Draw Time");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f ms", sceneStats.gpuDrawTime);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("GPU Busy");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f%%", sceneStats.gpuBusy);
                EditorUI::HoverToolTip("How much of your frame time the GPU was actively doing work you submitted.");
#endif
                ImGui::EndTable();
            }
        }

        ImGui::SeparatorText("Output");
        {
            if (ImGui::BeginTable("OutputTable", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Resolution");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u x %u", state.swapchain->width, state.swapchain->height);
                EditorUI::HoverToolTip("Current swapchain/backbuffer resolution");

                ImGui::EndTable();
            }
        }

        ImGui::SeparatorText("Scene Stats");
        {
            if (ImGui::BeginTable("SceneStatsTable", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Meshes");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", state.sceneStats->totalMeshCount);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Draw Calls");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", state.sceneStats->drawCallCount);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Vertices");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", state.sceneStats->totalVerts);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Triangles");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", state.sceneStats->totalTris);

                ImGui::EndTable();
            }
        }

        // GPU Info
        if (state.showGPUInfo)
        {
            ImGui::SeparatorText("GPU Overview");

            if (ImGui::BeginTable("Overview", 2, ImGuiTableFlags_SizingStretchProp))
            {
                // Make first column a little narrower, second stretches
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                // GPU Name
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("GPU");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", state.device->deviceDesc.name.c_str());

                // Driver Version
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Driver Ver");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", state.device->deviceDesc.driverVersionString.c_str());

                // API Version
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("API");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", state.device->deviceDesc.apiName.c_str());

                // VRAM
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("VRAM");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f GB", state.device->deviceDesc.dedicatedVideoMemory / (1024.0f * 1024.0f * 1024.0f));

                ImGui::EndTable();
            }
        }

        // Dynamic tables from toggles
        if (ImGui::BeginTable("TimingTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            if (showDepthRange)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SeparatorText("Depth Buffer View Options");
                ImGui::TableSetColumnIndex(1);
                ImGui::Dummy(ImVec2(0, 0)); // keep layout happy

                // Depth Range row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Depth Range");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                float range = std::clamp(state.debugData->debugDepthRange, 1.0f, 1000.0f);
                if (ImGui::SliderFloat("##DepthRange", &range, 1.0f, 1000.0f, "%.1f", ImGuiSliderFlags_Logarithmic))
                    state.debugData->debugDepthRange = range;
                ImGui::PopItemWidth();
            }

            if (state.debugRenderer->enabled)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::SeparatorText("AABB Settings");
                ImGui::TableSetColumnIndex(1);
                ImGui::Dummy(ImVec2(0, 0));

                // Depth bias
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Depth Bias");
                ImGui::TableSetColumnIndex(1);
                static float depthBias = 0.0f;
                if (ImGui::SliderFloat("##AABBBias", &depthBias, 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic))
                {
                    state.debugRenderer->SetDepthBias(depthBias);
                }

                // Color
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Color");
                ImGui::TableSetColumnIndex(1);
                static glm::vec4 color = {1.0f, 1.0f, 0.0f, 1.0f};
                if (ImGui::ColorButton("##AABBColor", ImVec4(color.x, color.y, color.z, color.w),
                                       ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar, ImVec2(40, 20)))
                {
                    ImGui::OpenPopup("AABBColorPicker");
                }
                if (ImGui::BeginPopup("AABBColorPicker"))
                {
                    if (ImGui::ColorPicker4("##AABBColorPicker", &color.x,
                                            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB))
                    {
                        state.debugRenderer->SetColor(color);
                    }
                    ImGui::EndPopup();
                }

                // Reset
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Button("Reset"))
                {
                    depthBias = 1e-4f;
                    color = {1.f, 1.f, 0.f, 1.f};
                    state.debugRenderer->SetDepthBias(depthBias);
                    state.debugRenderer->SetColor(color);
                    state.debugRenderer->SetFlags(0);
                }
            }

            ImGui::EndTable();
        }

        // Shader profiling options
        ImGui::SeparatorText("Scene Shader Profiling");
        {
            if (ImGui::BeginTable("ShaderToggles", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                // ImGui::TableNextRow();
                // ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Disable Normal Map");
                // ImGui::TableSetColumnIndex(1);
                // {
                // 	bool dn = (debugData.disableNormalMap != 0);
                // 	if (ImGui::Checkbox("##DisableNormal", &dn))
                // 		debugData.disableNormalMap = dn ? 1u : 0u;
                // 	EditorUI::HoverToolTip("Skip sampling/decoding the normal map to profile fragment cost.");
                // }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Disable Specular");
                ImGui::TableSetColumnIndex(1);
                {
                    bool ds = (state.debugData->disableSpecular != 0);
                    if (ImGui::Checkbox("##DisableSpecular", &ds))
                        state.debugData->disableSpecular = ds ? 1u : 0u;
                    EditorUI::HoverToolTip("Turn off the specular term to measure its impact.");
                }

                ImGui::EndTable();
            }

            // PBR/IBL Tuning Section
            ImGui::Spacing();
            ImGui::SeparatorText("PBR/IBL Tuning");

            ImGui::SliderFloat("IBL Strength", &state.debugData->iblStrength, 0.0f, 3.0f, "%.2f");
            EditorUI::HoverToolTip("Overall strength of image-based lighting reflections");

            ImGui::SliderFloat("IBL Mip Bias", &state.debugData->iblRoughnessMipBias, -2.0f, 2.0f, "%.2f");
            EditorUI::HoverToolTip("Offset for roughness mip selection (negative = sharper reflections)");

            ImGui::SliderFloat("Ambient Strength", &state.debugData->ambientStrength, 0.0f, 0.2f, "%.3f");
            EditorUI::HoverToolTip("Base ambient lighting color contribution");

            ImGui::SliderFloat("AO Strength", &state.debugData->aoStrength, 0.0f, 1.0f, "%.2f");
            EditorUI::HoverToolTip("Ambient occlusion multiplier (affects ambient & IBL)");

            ImGui::SliderFloat("Metallic Reflect Scale", &state.debugData->metallicReflectScale, 0.0f, 2.0f, "%.2f");
            EditorUI::HoverToolTip("Scale metallic material reflections");

            ImGui::SliderFloat("Roughness Reflect Scale", &state.debugData->roughnessReflectScale, 0.0f, 1.0f, "%.2f");
            EditorUI::HoverToolTip("How much roughness reduces reflection strength");
        }

        // Allows it so you can draw it anywhere
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Top-left", nullptr, corner == TopLeft)) corner = TopLeft;
            if (ImGui::MenuItem("Top-right", nullptr, corner == TopRight)) corner = TopRight;
            if (ImGui::MenuItem("Bottom-left", nullptr, corner == BottomLeft)) corner = BottomLeft;
            if (ImGui::MenuItem("Bottom-right", nullptr, corner == BottomRight)) corner = BottomRight;
            if (ImGui::MenuItem("Center", nullptr, corner == Center)) corner = Center;
            if (ImGui::MenuItem("Custom", nullptr, corner == Custom)) corner = Custom;
            ImGui::Separator();
            ImGui::EndPopup();
        }

        ImGui::End();
    }
}

void EditorUI::DrawInputDebugPanel()
{
    if (!ImGui::Begin("Input Debug"))
    {
        ImGui::End();
        return;
    }

    // --- Keyboard ---
    if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Using Keyboard: %s", input.usingKeyboard ? "Yes" : "No");
        ImGui::Separator();
        ImGui::BeginChild("KeyboardKeys", ImVec2(0, 180), true);

        for (int k = 0; k < Keyboard::ButtonCount; ++k)
        {
            const auto& key = input.keyboard[k];
            if (!key.pressed && !key.held && !key.released)
                continue;

            ImVec4 col;
            if (key.pressed)
                col = ImVec4(1.0f, 1.0f, 0.2f, 1.0f); // yellow = just pressed
            else if (key.held)
                col = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); // green = held
            else if (key.released)
                col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // red = released
            else
                col = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

            ImGui::TextColored(col, "Key %d (%s%s%s)",
                               k,
                               key.pressed ? "P" : "",
                               key.held ? "H" : "",
                               key.released ? "R" : "");
        }

        ImGui::EndChild();
    }

    // --- Mouse ---
    if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Using Mouse: %s", input.usingMouse ? "Yes" : "No");
        ImGui::Text("Pos: (%.1f, %.1f)", input.cursorX, input.cursorY);
        ImGui::Text("Delta: (%.1f, %.1f)", input.xrel, input.yrel);
        ImGui::Text("Scroll: (%d, %d)", input.scrollX, input.scrollY);
        ImGui::Separator();

        for (int i = 0; i < Mouse::Button::ButtonCount; ++i)
        {
            const char* btnNames[] = {"Left", "Right", "Middle", "Button4", "Button5"};
            const auto& btn = input.mouseButtons[i];
            ImVec4 col;
            if (btn.pressed)
                col = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
            else if (btn.held)
                col = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
            else if (btn.released)
                col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            else
                col = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

            ImGui::TextColored(col, "%s", btnNames[i]);
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }

    // --- Controllers ---
    if (ImGui::CollapsingHeader("Controllers", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (u32 i = 0; i < input.controllers.size(); ++i)
        {
            const auto& pad = input.controllers[i];
            if (!pad.connected) continue;

            ImGui::SeparatorText(("Controller " + std::to_string(i)).c_str());
            ImGui::Text("Connected: Yes");
            ImGui::Text("LTrigger: %.2f | RTrigger: %.2f", pad.leftTrigger, pad.rightTrigger);
            ImGui::Text("Left Stick: (%.2f, %.2f)", pad.leftX, pad.leftY);
            ImGui::Text("Right Stick: (%.2f, %.2f)", pad.rightX, pad.rightY);
            ImGui::Text("Vibration: L %.2f | R %.2f", pad.leftMotorVibration, pad.rightMotorVibration);
            ImGui::Spacing();

            // Visualize left stick movement
            constexpr float size = 60.0f;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 center(p.x + size * 0.5f, p.y + size * 0.5f);
            draw->AddCircle(center, size * 0.5f, IM_COL32(120, 120, 120, 255));
            draw->AddCircleFilled(
                ImVec2(center.x + pad.leftX * size * 0.4f,
                       center.y - pad.leftY * size * 0.4f),
                5.0f, IM_COL32(0, 255, 0, 255));
            ImGui::Dummy(ImVec2(size, size));

            // Button visualization
            ImGui::SeparatorText("Buttons");
            ImGui::BeginChild(("PadButtons" + std::to_string(i)).c_str(), ImVec2(0, 120), true);
            for (int b = 0; b < Gamepad::Button::ButtonCount; ++b)
            {
                const auto& btn = pad.buttons[b];
                if (!btn.pressed && !btn.held && !btn.released) continue;

                ImVec4 col;
                if (btn.pressed)
                    col = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
                else if (btn.held)
                    col = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
                else if (btn.released)
                    col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                else
                    col = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

                ImGui::TextColored(col, "Button %d (%s%s%s)",
                                   b,
                                   btn.pressed ? "P" : "",
                                   btn.held ? "H" : "",
                                   btn.released ? "R" : "");
            }
            ImGui::EndChild();
        }
    }

    ImGui::End();
}


// Project a world-space point to screen space (returns false if behind camera or off-screen)
static bool ProjectToScreen(const glm::vec3& worldPos, const glm::mat4& view, const glm::mat4& proj, ImVec2& outScreen)
{
    const glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.0f) return false; // behind camera
    const glm::vec3 ndc = clip / clip.w;
    // Optional: cull if far outside [-1,1]
    if (ndc.x < -1.2f || ndc.x > 1.2f || ndc.y < -1.2f || ndc.y > 1.2f) return false;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float sx = (ndc.x * 0.5f + 0.5f) * vp->Size.x;
    const float sy = (ndc.y * 0.5f + 0.5f) * vp->Size.y; // Vulkan-style proj already Y-flipped
    outScreen = {vp->Pos.x + sx, vp->Pos.y + sy};
    return true;
}

static LightUBO MakeDirectional()
{
    return {
        .direction = glm::normalize(glm::vec3{-0.5f, -1.f, -0.3f}),
        .color = {1.f, 0.95f, 0.85f},
        .intensity = 3.f,
        .type = static_cast<u32>(LightType::Directional),
    };
}

static LightUBO MakePoint(const Camera* cam)
{
    return {
        .position = cam->position + cam->forward * 2.f,
        .range = 8.f,
        .innerCone = 0.f,
        .color = {1.f, 1.f, 1.f},
        .intensity = 5.f,
        .type = static_cast<u32>(LightType::Point),
        .outerCone = 0.f
    };
}


static LightUBO MakeSpot(const Camera* cam)
{
    return {
        .position = cam->position,
        .range = 12.f,
        .direction = glm::normalize(cam->forward),
        .innerCone = std::cos(glm::radians(12.f)),
        .color = {1.f, 1.f, 1.f},
        .intensity = 6.f,
        .type = static_cast<u32>(LightType::Spot),
        .outerCone = std::cos(glm::radians(20.f))
    };
}

void EditorUI::DrawLightEditor() const
{
    const Camera* cam = state.activeCamera;
    auto& lights = *state.lights;

    // Little helper: Draw "+Directional/Point/Spot"
    auto DrawAddButtons = [&]()
    {
        if (ImGui::Button("+ Directional")) lights.push_back(MakeDirectional());
        ImGui::SameLine();
        if (ImGui::Button("+ Point")) lights.push_back(MakePoint(cam));
        ImGui::SameLine();
        if (ImGui::Button("+ Spot")) lights.push_back(MakeSpot(cam));
    };


    if (lights.empty())
    {
        ImGui::Begin("Light Editor");
        ImGui::TextUnformatted("No lights in scene.");
        DrawAddButtons();
        ImGui::End();
        return;
    }

    ImGui::Begin("Light Editor");
    DrawAddButtons();
    ImGui::Separator();

    // Selection list
    static int selected = 0;
    selected = glm::clamp(selected, 0, static_cast<int>(lights.size()) - 1);

    if (ImGui::BeginListBox("Lights"))
    {
        for (int i = 0; i < lights.size(); ++i)
        {
            const char* tn =
                (lights[i].type == static_cast<u32>(LightType::Directional))
                    ? "Dir"
                    : (lights[i].type == static_cast<u32>(LightType::Point))
                    ? "Point"
                    : "Spot";

            char label[32];
            std::snprintf(label, sizeof(label), "%s %d", tn, i);

            if (ImGui::Selectable(label, selected == i))
                selected = i;
        }
        ImGui::EndListBox();
    }

    LightUBO& L = lights[selected];


    if (ImGui::Button("Remove Selected"))
    {
        lights.erase(lights.begin() + selected);

        if (lights.empty())
        {
            ImGui::End();
            return;
        }

        selected = std::max(0, selected - 1);
    }

    ImGui::SameLine();
    if (ImGui::Button("Duplicate"))
        lights.push_back(L);


    ImGui::SeparatorText("Properties");

    static const char* TypeNames[] = {"Directional", "Point", "Spot"};
    ImGui::Text("Type: %s", TypeNames[L.type]);

    ImGui::ColorEdit3("Color", &L.color.x);
    ImGui::DragFloat("Intensity", &L.intensity, 0.1f, 0.f, 1000.f);

    if (L.type != LightType::Directional)
    {
        ImGui::DragFloat3("Position", &L.position.x, 0.05f);
        ImGui::DragFloat("Range", &L.range, 0.1f, 0.f, 5000.f);
    }

    if (L.type != LightType::Point)
    {
        ImGui::DragFloat3("Direction", &L.direction.x, 0.01f, -1.f, 1.f);
        if (glm::length2(L.direction) > 1e-6f)
            L.direction = glm::normalize(L.direction);

        if (L.type == LightType::Spot)
        {
            float innerDeg = glm::degrees(std::acos(std::clamp(L.innerCone, -1.f, 1.f)));
            float outerDeg = glm::degrees(std::acos(std::clamp(L.outerCone, -1.f, 1.f)));

            if (ImGui::SliderFloat("Inner Cone (deg)", &innerDeg, 0.f, 80.f))
                L.innerCone = std::cos(glm::radians(innerDeg));

            if (ImGui::SliderFloat("Outer Cone (deg)", &outerDeg, innerDeg + 1.f, 89.f))
                L.outerCone = std::cos(glm::radians(outerDeg));
        }
    }

    static bool showOverlay = true;
    ImGui::Checkbox("Show overlay gizmos", &showOverlay);

    ImGui::SameLine();
    if (ImGui::Button("Snap Selected To Camera"))
    {
        if (L.type != LightType::Directional)
            L.position = cam->position;

        if (L.type != LightType::Point)
            L.direction = glm::normalize(cam->forward);
    }

    ImGui::End();

    if (!showOverlay) return;


    // 2D Gizmos Overlay
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const glm::mat4 view = cam->GetViewMatrix();
    const glm::mat4 proj = cam->GetProjectionMatrix(
        static_cast<f32>(state.swapchain->width) / static_cast<f32>(state.swapchain->height));

    for (int i = 0; i < lights.size(); ++i)
    {
        const LightUBO& Li = lights[i];
        const bool isSel = (i == selected);

        ImU32 baseColor =
            (Li.type == LightType::Directional)
                ? IM_COL32(255, 220, 100, 255)
                : (Li.type == LightType::Point)
                ? IM_COL32(120, 200, 255, 255)
                : IM_COL32(180, 255, 120, 255);

        const float radius = isSel ? 8.f : 6.f;
        const ImU32 outline = isSel
                                  ? IM_COL32(255, 255, 200, 255)
                                  : IM_COL32(0, 0, 0, 180);

        // Directional light gizmo
        if (Li.type == LightType::Directional)
        {
            glm::vec3 A = cam->position;
            glm::vec3 B = cam->position - glm::normalize(Li.direction) * 2.f;

            ImVec2 sA, sB;
            if (ProjectToScreen(A, view, proj, sA) &&
                ProjectToScreen(B, view, proj, sB))
            {
                if (isSel)
                    dl->AddCircle(sA, radius * 1.8f, IM_COL32(255, 255, 180, 100), 24, 2.f);

                dl->AddCircleFilled(sA, radius, baseColor);
                dl->AddCircle(sA, radius, outline);
                dl->AddLine(sA, sB, baseColor, isSel ? 2.5f : 2.f);
                dl->AddText({sA.x + 8, sA.y - 10}, baseColor, "Dir");
            }
        }
        else // Point or Spot
        {
            ImVec2 sP;
            if (!ProjectToScreen(Li.position, view, proj, sP))
                continue;

            if (isSel)
                dl->AddCircle(sP, radius * 1.8f, IM_COL32(255, 255, 180, 100), 24, 2.f);

            dl->AddCircleFilled(sP, radius, baseColor);
            dl->AddCircle(sP, radius, outline);

            const char* lbl = (Li.type == LightType::Spot) ? "Spot" : "Point";
            dl->AddText({sP.x + 8, sP.y - 10}, baseColor, lbl);

            if (Li.type == LightType::Spot)
            {
                glm::vec3 tip =
                    Li.position +
                    glm::normalize(Li.direction) *
                    0.8f * glm::max(Li.range, 0.5f);

                ImVec2 sQ;
                if (ProjectToScreen(tip, view, proj, sQ))
                    dl->AddLine(sP, sQ, baseColor, isSel ? 2.5f : 2.f);
            }
        }
    }
}