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
#include "ufbx.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"
#ifndef IMGUI_DISABLE
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#if ENGINE_PLATFORM_WIN32
#define VOLK_IMPLEMENTATION
#include <backends/imgui_impl_win32.h>
#elif ENGINE_PLATFORM_SDL
#include <SDL3/SDL_version.h>
#include <backends/imgui_impl_sdl3.h>
#include "SDL3/SDL_vulkan.h"
#endif

#endif

inline ImVec4 ToLinear(ImVec4 col)
{
    // Approximation: Gamma 2.2 curve
    return ImVec4(
        std::pow(col.x, 2.2f),
        std::pow(col.y, 2.2f),
        std::pow(col.z, 2.2f),
        col.w // Alpha stays linear
    );
}

void EditorUI::InitEditorStyles()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Catppuccin Mocha Palette
    // --------------------------------------------------------
    const ImVec4 base     = ToLinear(ImVec4(0.117f, 0.117f, 0.172f, 1.0f)); // #1e1e2e
    const ImVec4 mantle   = ToLinear(ImVec4(0.109f, 0.109f, 0.156f, 1.0f)); // #181825
    const ImVec4 surface0 = ToLinear(ImVec4(0.200f, 0.207f, 0.286f, 1.0f)); // #313244
    const ImVec4 surface1 = ToLinear(ImVec4(0.247f, 0.254f, 0.337f, 1.0f)); // #3f4056
    const ImVec4 surface2 = ToLinear(ImVec4(0.290f, 0.301f, 0.388f, 1.0f)); // #4a4d63
    const ImVec4 overlay0 = ToLinear(ImVec4(0.396f, 0.403f, 0.486f, 1.0f)); // #65677c
    const ImVec4 overlay2 = ToLinear(ImVec4(0.576f, 0.584f, 0.654f, 1.0f)); // #9399b2
    const ImVec4 text     = ToLinear(ImVec4(0.803f, 0.815f, 0.878f, 1.0f)); // #cdd6f4
    const ImVec4 subtext0 = ToLinear(ImVec4(0.639f, 0.658f, 0.764f, 1.0f)); // #a3a8c3
    const ImVec4 mauve    = ToLinear(ImVec4(0.796f, 0.698f, 0.972f, 1.0f)); // #cba6f7
    const ImVec4 peach    = ToLinear(ImVec4(0.980f, 0.709f, 0.572f, 1.0f)); // #fab387
    const ImVec4 yellow   = ToLinear(ImVec4(0.980f, 0.913f, 0.596f, 1.0f)); // #f9e2af
    const ImVec4 green    = ToLinear(ImVec4(0.650f, 0.890f, 0.631f, 1.0f)); // #a6e3a1
    const ImVec4 teal     = ToLinear(ImVec4(0.580f, 0.886f, 0.819f, 1.0f)); // #94e2d5
    const ImVec4 sapphire = ToLinear(ImVec4(0.458f, 0.784f, 0.878f, 1.0f)); // #74c7ec
    const ImVec4 blue     = ToLinear(ImVec4(0.533f, 0.698f, 0.976f, 1.0f)); // #89b4fa
    const ImVec4 lavender = ToLinear(ImVec4(0.709f, 0.764f, 0.980f, 1.0f)); // #b4befe

    // Main window and backgrounds
    colors[ImGuiCol_WindowBg] = base;
    colors[ImGuiCol_ChildBg] = base;
    colors[ImGuiCol_PopupBg] = surface0;
    colors[ImGuiCol_Border] = surface1;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = surface0;
    colors[ImGuiCol_FrameBgHovered] = surface1;
    colors[ImGuiCol_FrameBgActive] = surface2;
    colors[ImGuiCol_TitleBg] = mantle;
    colors[ImGuiCol_TitleBgActive] = surface0;
    colors[ImGuiCol_TitleBgCollapsed] = mantle;
    colors[ImGuiCol_MenuBarBg] = mantle;
    colors[ImGuiCol_ScrollbarBg] = surface0;
    colors[ImGuiCol_ScrollbarGrab] = surface2;
    colors[ImGuiCol_ScrollbarGrabHovered] = overlay0;
    colors[ImGuiCol_ScrollbarGrabActive] = overlay2;
    colors[ImGuiCol_CheckMark] = green;
    colors[ImGuiCol_SliderGrab] = sapphire;
    colors[ImGuiCol_SliderGrabActive] = blue;
    colors[ImGuiCol_Button] = surface0;
    colors[ImGuiCol_ButtonHovered] = surface1;
    colors[ImGuiCol_ButtonActive] = surface2;
    colors[ImGuiCol_Header] = surface0;
    colors[ImGuiCol_HeaderHovered] = surface1;
    colors[ImGuiCol_HeaderActive] = surface2;
    colors[ImGuiCol_Separator] = surface1;
    colors[ImGuiCol_SeparatorHovered] = mauve;
    colors[ImGuiCol_SeparatorActive] = mauve;
    colors[ImGuiCol_ResizeGrip] = surface2;
    colors[ImGuiCol_ResizeGripHovered] = mauve;
    colors[ImGuiCol_ResizeGripActive] = mauve;
    colors[ImGuiCol_Tab] = surface0;
    colors[ImGuiCol_TabHovered] = surface2;
    colors[ImGuiCol_TabActive] = surface1;
    colors[ImGuiCol_TabUnfocused] = surface0;
    colors[ImGuiCol_TabUnfocusedActive] = surface1;
    colors[ImGuiCol_DockingPreview] = sapphire;
    colors[ImGuiCol_DockingEmptyBg] = base;
    colors[ImGuiCol_PlotLines] = blue;
    colors[ImGuiCol_PlotLinesHovered] = peach;
    colors[ImGuiCol_PlotHistogram] = teal;
    colors[ImGuiCol_PlotHistogramHovered] = green;
    colors[ImGuiCol_TableHeaderBg] = surface0;
    colors[ImGuiCol_TableBorderStrong] = surface1;
    colors[ImGuiCol_TableBorderLight] = surface0;
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = surface2;
    colors[ImGuiCol_DragDropTarget] = yellow;
    colors[ImGuiCol_NavHighlight] = lavender;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = subtext0;

    // Rounded corners
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    // Padding and spacing
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(5.0f, 3.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // Borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
}

// For multi-viewport support
#if ENGINE_PLATFORM_WIN32
static i32 CreateVulkanSurfaceForImGui(ImGuiViewport* vp, ImU64 vkInst, const void* vkAlloc = nullptr,
                                       ImU64* outVkSurface = nullptr)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = (HWND)vp->PlatformHandleRaw;
    createInfo.hinstance = ::GetModuleHandle(nullptr);

    return vkCreateWin32SurfaceKHR((VkInstance)vkInst, &createInfo, (const VkAllocationCallbacks*)vkAlloc,
                                   (VkSurfaceKHR*)outVkSurface);
}

#elif ENGINE_PLATFORM_SDL

static i32 CreateVulkanSurfaceForImGui(ImGuiViewport* vp, ImU64 vkInst, const void* vkAlloc = nullptr, ImU64* outVkSurface = nullptr)
{
    if (auto* win = static_cast<SDL_Window*>(vp->PlatformHandleRaw); !SDL_Vulkan_CreateSurface(win, (VkInstance)vkInst, nullptr, (VkSurfaceKHR*)outVkSurface))
        return -1;

    return 0;
}

#endif

bool EditorUI::Init(Renderer::GPUInterface* instance, Renderer::GPUDevice* device, Renderer::GPUSwapchain* swapchain)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.ConfigDebugHighlightIdConflicts = true;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    InitEditorStyles();

#if defined(ENGINE_PLATFORM_WIN32)
    ImGui_ImplWin32_Init(swapchain->GetWindowHandle());
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateVkSurface = CreateVulkanSurfaceForImGui;
#elif defined(ENGINE_PLATFORM_SDL)
    auto* sdlWin = static_cast<SDL_Window*>(swapchain->GetWindowHandle());
    ImGui_ImplSDL3_InitForVulkan(sdlWin);
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_CreateVkSurface = CreateVulkanSurfaceForImGui;
#endif

#ifdef VULKAN_BUILD
    auto* vkDev = static_cast<Renderer::VulkanDevice*>(device);
    auto* vkInst = static_cast<Renderer::VulkanInstance*>(instance);

    ImGui_ImplVulkan_InitInfo initInfo = {
        .ApiVersion = vkInst->appInfo.apiVersion,
        .Instance = vkInst->instance,
        .PhysicalDevice = vkDev->physicalDevice,
        .Device = vkDev->device,
        .QueueFamily = vkDev->graphicsQueueIndex,
        .Queue = vkDev->graphicsQueue,
        .DescriptorPool = VK_NULL_HANDLE,
        .DescriptorPoolSize = 1000,
        .MinImageCount = 2,
        .ImageCount = MAX_FRAME_OVERLAP,
        .UseDynamicRendering = true,
    };
    static constexpr VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;

    ImGui_ImplVulkan_PipelineInfo pipelineInfo = {
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .PipelineRenderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchainFormat
        },
    };

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
#endif
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

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void EditorUI::Render(Renderer::GPUCommandBuffer* cmd)
{
    const auto* vkCmd = static_cast<Renderer::VulkanCommandBuffer*>(cmd);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmd->GetVkHandle());
}

void EditorUI::DrawCameraGizmo(CameraComponent& camComp)
{
    const glm::mat4 proj = glm::perspective(Radians(90.0f), 1.0f, 0.1f, 1000.0f);

    // Use viewport work area like stats overlay (accounts for menu bar and docking)
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    // Simple hardcoded sizes
    constexpr f32 gizmoSize = 125.0f;
    constexpr f32 padding = 10.0f;

    // Position in top-right corner of work area
    const f32 xPos = work_pos.x + work_size.x - gizmoSize - padding;
    const f32 yPos = work_pos.y + padding;

    // Draw directly using viewport foreground draw list (sticks to main viewport)
    ImOGuizmo::SetRect(xPos, yPos, gizmoSize);
    ImOGuizmo::SetDrawList(ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(viewport)));
    ImOGuizmo::BeginFrame();
    ImOGuizmo::DrawGizmo(glm::value_ptr(camComp.base.view), glm::value_ptr(proj), 5.0f);
}

void EditorUI::DrawCameraEditor()
{
    if (state.freezeFrustum && state.frozenCam)
    {
        ImGui::SeparatorText("Frustum Debugger");

        bool isFrozen = *state.freezeFrustum;

        if (ImGui::Checkbox("Freeze Frustum in Place (F7)", &isFrozen))
        {
            *state.freezeFrustum = isFrozen;

            // If turning ON, snapshot the live camera immediately
            if (isFrozen)
            {
                *state.frozenCam = state.cameraComponents[state.activeCameraIdx];
            }
        }

        if (isFrozen)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), " [EDITING GHOST]");

            CameraComponent* ghost = state.frozenCam;
            bool changed = false;

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));

            // --- Position Edit ---
            if (ImGui::DragFloat3("Ghost Pos", glm::value_ptr(ghost->position), 0.1f))
                changed = true;

            // --- Rotation Edit ---
            if (ImGui::SliderFloat("Ghost Yaw", &ghost->base.yaw, -180.0f, 180.0f))
                changed = true;

            if (ImGui::SliderFloat("Ghost Pitch", &ghost->base.pitch, -89.0f, 89.0f))
                changed = true;

            // --- FOV Edit (Fun for testing culling) ---
            if (ImGui::SliderFloat("Ghost FOV", &ghost->base.fov, 10.0f, 160.0f))
                changed = true;

            ImGui::PopStyleColor();

            // If we moved the sliders, we MUST rebuild the view/projection matrix
            if (changed)
            {
                const ImGuiViewport* vp = ImGui::GetMainViewport();
                const f32 aspect = vp->WorkSize.x / vp->WorkSize.y;
                ghost->base.UpdateVecAndMat(ghost->position, aspect);
            }

            if (ImGui::Button("Teleport Real Cam to Ghost"))
            {
                CameraComponent& active = state.cameraComponents[state.activeCameraIdx];
                active.position = ghost->position;
                active.base.yaw = ghost->base.yaw;
                active.base.pitch = ghost->base.pitch;
                active.base.fov = ghost->base.fov;

                // Snap the real camera's matrices too
                const ImGuiViewport* vp = ImGui::GetMainViewport();
                active.base.UpdateVecAndMat(active.position, vp->WorkSize.x / vp->WorkSize.y);
            }
        }
        ImGui::Separator();
    }


    if (ImGui::BeginListBox("##SceneCameras", ImVec2(-FLT_MIN, 120)))
    {
        for (u32 i = 0; i < state.cameraComponents.size(); ++i)
        {
            const bool isActive = (i == state.activeCameraIdx);
            const bool isSelected = (i == state.selectedCameraIdx);
            char label[64];
            std::snprintf(label, sizeof(label), "%s Camera %u %s",
                            isActive ? "[ACTIVE]" : "        ",
                            i,
                            isSelected ? "(Selected)" : "");

            if (ImGui::Selectable(label, isSelected))
            {
                state.selectedCameraIdx = i;
            }

            // Double click to possess immediately
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                state.activeCameraIdx = i;
            }
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();

    const u32 selIdx = state.selectedCameraIdx;
    if (selIdx < state.cameraComponents.size())
    {
        CameraComponent& targetComp = state.cameraComponents[selIdx];

        if (ImGui::Button("Possess Camera")) state.activeCameraIdx = selIdx;
        ImGui::SameLine();
        if (ImGui::Button("Snap to Me"))
        {
            const CameraComponent& activeComp = state.cameraComponents[state.activeCameraIdx];
            targetComp.position = activeComp.position;
            targetComp.base.yaw = activeComp.base.yaw;
            targetComp.base.pitch = activeComp.base.pitch;

            const ImGuiViewport* vp = ImGui::GetMainViewport();
            const f32 aspect = vp->WorkSize.x / vp->WorkSize.y;
            targetComp.base.UpdateVecAndMat(targetComp.position, aspect);
        }

        DrawCameraProperties(targetComp);
    }
}

void EditorUI::DrawCameraProperties(CameraComponent& camComp)
{
    ImGui::SeparatorText("Selected Camera Settings");

    if (ImGui::BeginTable("CameraProps", 2, ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Position");
        ImGui::TableSetColumnIndex(1);
        ImGui::DragFloat3("##pos", glm::value_ptr(camComp.position), 0.1f);
        if (ImGui::SmallButton("Reset Pos")) camComp.position = {0, 5, 0};

        // --- FOV ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("FOV");
        ImGui::TableSetColumnIndex(1);
        ImGui::SliderFloat("##CameraFOV", &camComp.base.fov, 5.0f, 160.0f);

        // --- Clipping Planes ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Near/Far");
        ImGui::TableSetColumnIndex(1);
        ImGui::InputFloat("Near", &camComp.base.nearPlane);
        ImGui::InputFloat("Far", &camComp.base.farPlane);

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

void EditorUI::ClampWindowToViewport()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;

    // Keep window within viewport bounds
    pos.x = std::clamp(pos.x, work_pos.x, work_pos.x + work_size.x - size.x);
    pos.y = std::clamp(pos.y, work_pos.y, work_pos.y + work_size.y - size.y);

    ImGui::SetWindowPos(pos);
}

void EditorUI::DrawCameraSpeedPopup(f32 camSpeedPopupTime) const
{
    if (camSpeedPopupTime <= 0.0f) return;

    // Use viewport work area to position relative to usable screen space
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    // Position in top-center of work area (12% down from top)
    const ImVec2 pos = {work_pos.x + (work_size.x * 0.5f), work_pos.y + (work_size.y * 0.12f)};

    const f32 alpha = std::min(1.0f, camSpeedPopupTime / 1.0f);

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
    ImGui::End();
}

void EditorUI::DrawDebugViewPopup(f32 debugViewPopupTime, DebugView currentView)
{
    if (debugViewPopupTime <= 0.0f) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    // Position in center of work area (slightly higher than center: 40% down)
    const ImVec2 pos = {work_pos.x + (work_size.x * 0.5f), work_pos.y + (work_size.y * 0.20f)};

    // Fade out over the last 0.5 seconds
    const f32 alpha = std::min(1.0f, debugViewPopupTime / 0.5f);

    ImGui::SetNextWindowBgAlpha(alpha * 0.7f); // Slightly transparent background
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration       |
        ImGuiWindowFlags_AlwaysAutoResize   |
        ImGuiWindowFlags_NoSavedSettings    |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav              |
        ImGuiWindowFlags_NoInputs           |
        ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##DebugViewPopup", nullptr, flags))
    {
        ImGui::Text("View Mode");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", DebugViewToString(currentView));
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
}

bool EditorUI::DrawMainMenuBar()
{
    if (state.noUI)
        return false;

    bool shouldExit = false;

    if (!state.showMenuBar)
        return shouldExit;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("No UI Mode", "F6")) { state.noUI = true; }
            HoverToolTip("Hides the entire editor interface. Press F6 to restore.");
            if (ImGui::MenuItem("Exit"))
            {
                shouldExit = true;
            }
            HoverToolTip("Close this application?");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Show Main Overlay", "F2", &state.showMainOverlay);
            ImGui::MenuItem("Show Menu Bar", "F3", &state.showMenuBar);
            ImGui::MenuItem("Show GPU Info", "F4", &state.showGPUInfo);
            ImGui::MenuItem("Show Editor Tools", "T", &state.showEditorTools);

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
                    const bool selected = (state.swapchain->GetPresentMode() == mode);
                    if (ImGui::MenuItem(label, nullptr, selected))
                    {
                        state.swapchain->SetVsyncMode(mode);
                    }
                    if (selected) { ImGui::SetItemDefaultFocus(); }
                }
                HoverToolTip("Changes the swapchain presentation mode immediately.");
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About", nullptr, &state.showAboutPopup);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    return shouldExit;
}

void EditorUI::UpdateAlphaLerp(f32& currentAlpha, f32 minAlpha, f32 maxAlpha, f32 speed)
{
    const ImGuiIO& io = ImGui::GetIO();
    // Check if the current window being drawn is hovered
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup);

    const f32 target = hovered ? maxAlpha : minAlpha;

    if (std::abs(currentAlpha - target) > 0.001f)
    {
        currentAlpha = std::lerp(currentAlpha, target, std::clamp(speed * io.DeltaTime, 0.0f, 1.0f));
    }
    else
    {
        currentAlpha = target;
    }
}

void EditorUI::DrawMainOverlay() const
{
    if (state.noUI || !state.showMainOverlay) return;
    using namespace ImGui;

    static f32 overlayAlpha = 0.7f;

    const bool showDepthRange =
        state.debugData->debugMode == DebugView::Depth;

    enum Corner { Custom = -1, TopLeft, TopRight, BottomLeft, BottomRight, Center };
    static i32 corner = TopLeft;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 work_pos = vp->WorkPos;
    const ImVec2 work_size = vp->WorkSize;

    // Scale pad with DPI/work area; clamp to reasonable range.
    const f32 padding = std::clamp(10.0f * vp->DpiScale, 8.0f, 24.0f);

    ImVec2 window_pos;
    ImVec2 window_pivot;

    if (corner == TopLeft)
    {
        window_pos = ImVec2(work_pos.x + padding, work_pos.y + padding);
        window_pivot = ImVec2(0.0f, 0.0f);
    }
    else if (corner == TopRight)
    {
        window_pos = ImVec2(work_pos.x + work_size.x - padding, work_pos.y + padding);
        window_pivot = ImVec2(1.0f, 0.0f);
    }
    else if (corner == BottomLeft)
    {
        window_pos = ImVec2(work_pos.x + padding, work_pos.y + work_size.y - padding);
        window_pivot = ImVec2(0.0f, 1.0f);
    }
    else if (corner == BottomRight)
    {
        window_pos = ImVec2(work_pos.x + work_size.x - padding, work_pos.y + work_size.y - padding);
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
    f32 extra_rows = 0.0f;
    if (showDepthRange) extra_rows += 1.0f;
    if (state.debugRenderer->enabled) extra_rows += 5.0f;
    min_size.y += extra_rows * ImGui::GetFrameHeightWithSpacing();

    // Max size = screen area minus padding
    const ImVec2 max_size = ImVec2(
        std::max(150.0f, work_size.x - padding * 2.0f),
        std::max(80.0f, work_size.y - padding * 2.0f)
    );

    // Clamp again just to be safe
    min_size.x = std::clamp(min_size.x, 150.0f, max_size.x);
    min_size.y = std::clamp(min_size.y, 80.0f, max_size.y);

    ImGui::SetNextWindowBgAlpha(overlayAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, overlayAlpha);

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
        UpdateAlphaLerp(overlayAlpha, 0.6f, 1.0f, 8.0f);
        ClampWindowToViewport();

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
                EditorUI::HoverToolTip("Total CPU frame time (Update + Render Submission + Present).");
#ifdef ENABLE_GPU_TIMING
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("GPU Draw Time");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f ms", state.sceneStats->gpuDrawTime);
                EditorUI::HoverToolTip("Time taken by the GPU to execute the main render pass.");
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("GPU Busy");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f%%", state.sceneStats->gpuBusy);
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
                const Extent2D resolution = state.swapchain->GetExtent();
                ImGui::Text("%u x %u", resolution.width, resolution.height);
                EditorUI::HoverToolTip("Current swapchain/backbuffer resolution");

                ImGui::EndTable();
            }
        }

        ImGui::SeparatorText("Mesh / Input status");
        {
            if (ImGui::BeginTable("SceneStatsTable", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Visible Meshes");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", state.sceneStats->totalMeshCount);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Draw Calls");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", state.sceneStats->drawCallCount);


                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Triangles Rendered");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", state.sceneStats->totalTris);

                ImGui::EndTable();
            }
        }

        SeparatorText("Controls");
        {
            constexpr auto hintCol = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

            TextColored(hintCol, "F1      - Toggle Camera Mode (FPS/FreeFly)");
            TextColored(hintCol, "F2      - Toggle Debug Renderer");
            TextColored(hintCol, "F3      - Toggle Menu Bar");
            TextColored(hintCol, "F4      - Toggle GPU Info");
            TextColored(hintCol, "F5      - Toggle VSync");
            TextColored(hintCol, "F6      - Toggle UI Visibility");
            TextColored(hintCol, "F7      - Toggle Freeze Frustum");
            TextColored(hintCol, "F8      - Cycle Active Camera");
            TextColored(hintCol, "F9      - Cycle Debug View");
            TextColored(hintCol, "Alt     - Unlock Cursor (During FPS Mode)");
            TextColored(hintCol, "Scroll  - Change Camera Speed");
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
                ImGui::Text("%s", state.device->GetDeviceDesc().name.c_str());

                // Driver Version
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Driver Ver");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", state.device->GetDeviceDesc().driverVersionString.c_str());

                // API Version
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("API");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", state.device->GetDeviceDesc().apiName.c_str());

                // VRAM
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("VRAM");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f GB", static_cast<double>(state.device->GetDeviceDesc().dedicatedVideoMemory) / Gigabyte);

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
                ImGui::SeparatorText("Depth Buffer View");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Depth Range");

                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                f32 range = std::clamp(state.debugData->debugDepthRange, 1.0f, 1000.0f);
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
                static f32 depthBias = 0.0f;
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
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Reset AABB", ImVec2(-FLT_MIN, 0)))
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

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Disable Normal Map");
                ImGui::TableSetColumnIndex(1);
                {
                	bool dn = (state.debugData->disableNormalMap != 0);
                	if (ImGui::Checkbox("##DisableNormal", &dn))
                		state.debugData->disableNormalMap = dn ? 1u : 0u;
                	EditorUI::HoverToolTip("Skip sampling/decoding the normal map to profile fragment cost.");
                }

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
    ImGui::PopStyleVar();
}

void EditorUI::AppInfoPopup()
{
    if (!state.showAboutPopup) return;

    if (!ImGui::Begin("System Info", &state.showAboutPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text(" %s %s - %s", ENGINE_NAME, ENGINE_VERSION, ENGINE_BUILD_DATE);
    ImGui::SeparatorText("Third-Party Libraries");

    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("LibraryVersions", 2, flags)) {
        ImGui::TableSetupColumn("Library");
        ImGui::TableSetupColumn("Version");
        ImGui::TableHeadersRow();

        // Single string buffer for all formatted version strings
        char versionBuffer[64];

        auto AddLibRow = [](const char* name, const char* version) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", version);
        };

        // Static Getters
        AddLibRow("Dear ImGui", ImGui::GetVersion());

        snprintf(versionBuffer, sizeof(versionBuffer), "%d.%d.%d",
                 GLM_VERSION_MAJOR, GLM_VERSION_MINOR, GLM_VERSION_PATCH);
        AddLibRow("GLM", versionBuffer);


        snprintf(versionBuffer, sizeof(versionBuffer), "%u.%u.%u",
                 (uint32_t)(VMA_VERSION) >> 22U,
                 ((uint32_t)(VMA_VERSION) >> 12U) & 0x3FFU,
                 (uint32_t)(VMA_VERSION) & 0xFFFU);
        AddLibRow("VMA", versionBuffer);


#if ENGINE_PLATFORM_SDL
        const int linkedVer = SDL_GetVersion();
        snprintf(versionBuffer, sizeof(versionBuffer), "%d.%d.%d",
                 SDL_VERSIONNUM_MAJOR(linkedVer),
                 SDL_VERSIONNUM_MINOR(linkedVer),
                 SDL_VERSIONNUM_MICRO(linkedVer));
        AddLibRow("SDL3 (Linked)", versionBuffer);

        // SDL 3.5.0 Compile-time (Headers)
        snprintf(versionBuffer, sizeof(versionBuffer), "%d.%d.%d",
                 SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
        AddLibRow("SDL3 (Headers)", versionBuffer);

        // SDL Revision Hash
        AddLibRow("SDL3 (Revision)", SDL_GetRevision());
#endif


        // Miscellaneous
        constexpr uint32_t uv = UFBX_HEADER_VERSION;
        snprintf(versionBuffer, sizeof(versionBuffer), "%u.%u.%u",
                 uv / 10000, (uv / 100) % 100, uv % 100);
        AddLibRow("ufbx", versionBuffer);
#ifdef TRACY_ENABLE
        // Most Tracy versions define TRACY_VERSION or similar depending on the tag
        snprintf(versionBuffer, sizeof(versionBuffer), "%d.%d.%d",
                 TRACY_VERSION_MAJOR, TRACY_VERSION_MINOR, TRACY_VERSION_PATCH);
        AddLibRow("Tracy", versionBuffer);
#endif

        ImGui::EndTable();
    }

    ImGui::End();
}

void EditorUI::DrawEditorTools()
{
    if (state.noUI) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    constexpr f32 gizmoSize = 160.0f; // Matches DrawCameraGizmo
    constexpr f32 padding = 15.0f;

    // Tools window width
    constexpr f32 windowWidth = 400.0f;

    // Position: X is right-aligned minus window width and padding.
    // Y is top-aligned plus gizmo size and double padding.
    const f32 defaultX = viewport->WorkPos.x + viewport->WorkSize.x - windowWidth - padding;
    const f32 defaultY = viewport->WorkPos.y + padding + gizmoSize + padding;

    ImGui::SetNextWindowPos(ImVec2(defaultX, defaultY), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, 0.0f), ImGuiCond_Always); // 0 height + AutoResize = Snap to content

    ImGui::SetNextWindowBgAlpha(state.editorAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, state.editorAlpha);

    // AlwaysAutoResize is the secret to making it change size based on the active tab
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Editor Tools", &state.showEditorTools, flags))
    {
        UpdateAlphaLerp(state.editorAlpha, 0.6f, 1.0f, 8.0f);

        // Prevent window from being dragged outside the viewport
        ClampWindowToViewport();

        if (ImGui::BeginTabBar("EditorManagerTabs"))
        {
            if (ImGui::BeginTabItem("Lights"))
            {
                DrawLightEditor();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Camera"))
            {
                DrawCameraEditor();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    DrawLightGizmos(state.selectedLightIdx, state.cameraComponents[state.activeCameraIdx]);

    ImGui::End();
    ImGui::PopStyleVar();
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
    const f32 sx = (ndc.x * 0.5f + 0.5f) * vp->Size.x;
    const f32 sy = (ndc.y * 0.5f + 0.5f) * vp->Size.y; // Vulkan-style proj already Y-flipped
    outScreen = {vp->Pos.x + sx, vp->Pos.y + sy};
    return true;
}

static LightUBO MakeDirectional()
{
    return {
        .direction = glm::normalize(glm::vec3{-0.5f, -1.f, -0.3f}),
        .color = {1.f, 0.95f, 0.85f},
        .intensity = 3.f,
        .type = LightType::Directional,
    };
}

static LightUBO MakePoint(const glm::vec3& forward, const glm::vec3& position)
{
    return {
        .position = position + forward * 2.f,
        .range = 8.f,
        .innerCone = 0.f,
        .color = {1.f, 1.f, 1.f},
        .intensity = 5.f,
        .type = LightType::Point,
        .outerCone = 0.f
    };
}


static LightUBO MakeSpot(const glm::vec3& forward, const glm::vec3& position)
{
    return {
        .position = position,
        .range = 12.f,
        .direction = glm::normalize(forward),
        .innerCone = std::cos(glm::radians(12.f)),
        .color = {1.f, 1.f, 1.f},
        .intensity = 6.f,
        .type = LightType::Spot,
        .outerCone = std::cos(glm::radians(20.f))
    };
}

void EditorUI::UpdateLights(f32 deltaTime)
{
    auto& lights = *state.lights;
    const CameraComponent& activeCam = state.cameraComponents[state.activeCameraIdx];

    for (i32 i = 0; i < static_cast<i32>(lights.size()); ++i)
    {
        auto& L = lights[i];

        // Flashlight Logic: Sync spotlight to camera if toggled
        if (L.type == LightType::Spot && state.followCamera && static_cast<u32>(i) == state.selectedCameraIdx)
        {
            L.position = activeCam.position;
            L.direction = glm::normalize(activeCam.base.forward);
        }
    }

    // Global Animations (e.g., Spinning lights)
    if (state.spinLights && lights.size() >= 4) {
        state.currentLightTime += deltaTime * state.spinSpeed;
        for (i32 i = 0; i < 4; ++i) {
            const f32 angle = state.currentLightTime + (i * glm::half_pi<f32>());
            lights[i].position = glm::vec3(
                state.spinCenter.x + (state.spinRadius * std::sin(angle)),
                state.spinHeight,
                state.spinCenter.z + (state.spinRadius * std::cos(angle))
            );
        }
    }
}

void EditorUI::DrawLightGizmos(i32 selectedIdx, const CameraComponent& activeCam) const
{
    auto& lights = *state.lights;
    ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());

    const Camera& cam = activeCam.base;
    const glm::mat4& view = cam.view;
    const glm::mat4& proj = cam.projection;

    for (i32 i = 0; i < (i32)lights.size(); ++i)
    {
        const LightUBO& Li = lights[i];
        const bool isSel = (i == selectedIdx);

        // Color coding: Blue (Point), Green (Spot)
        const ImU32 col = (Li.type == 0) ? IM_COL32(255, 225, 100, 255)
                        : (Li.type == 1) ? IM_COL32(100, 200, 255, 255)
                        : IM_COL32(150, 255, 150, 255);

        ImVec2 sPos;
        glm::vec3 wPos = Li.position;

        if (!ProjectToScreen(wPos, view, proj, sPos)) continue;

        // 1. Selection Highlight (Glow Ring)
        if (isSel)
        {
            dl->AddCircle(sPos, 12.0f, col, 16, 2.0f);
            dl->AddCircle(sPos, 15.0f, IM_COL32(255, 255, 255, 120), 16, 1.0f);
        }
        dl->AddCircleFilled(sPos, isSel ? 7.0f : 4.0f, col);

        char idLabel[16];
        std::snprintf(idLabel, sizeof(idLabel), "%d", i);
        dl->AddText({sPos.x + 15, sPos.y - 15}, col, idLabel);
    }
}

void EditorUI::DrawLightEditor()
{
    // Retrieve the unified camera component for spawning references
    CameraComponent& activeCam = state.cameraComponents[state.activeCameraIdx];
    auto& lights = *state.lights;

    // Add New Lights
    if (ImGui::Button("+ Directional")) lights.push_back(MakeDirectional());
    ImGui::SameLine();
    if (ImGui::Button("+ Point")) lights.push_back(MakePoint(activeCam.base.forward, activeCam.position));
    ImGui::SameLine();
    if (ImGui::Button("+ Spot")) lights.push_back(MakeSpot(activeCam.base.forward, activeCam.position));

    ImGui::Separator();

    // Light Selection and Removal
    if (ImGui::BeginListBox("##LightsList", ImVec2(-FLT_MIN, 150)))
    {
        for (i32 i = 0; i < (i32)lights.size(); ++i)
        {
            const bool isSelected = (state.selectedLightIdx == (u32)i);
            const char* typeIcon = (lights[i].type == 0) ? "[D]" : (lights[i].type == 1) ? "[P]" : "[S]";

            char label[64];
            std::snprintf(label, sizeof(label), "%s Light %d", typeIcon, i);

            if (ImGui::Selectable(label, isSelected))
            {
                state.selectedLightIdx = i;
            }
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Global Spin Settings"))
    {
        ImGui::Checkbox("Enable Animation", &state.spinLights);
        ImGui::DragFloat("Speed", &state.spinSpeed, 0.05f);
        ImGui::DragFloat("Radius", &state.spinRadius, 0.1f);
        ImGui::DragFloat("Height", &state.spinHeight, 0.1f);
        ImGui::DragFloat3("Center", &state.spinCenter.x, 0.1f);
    }

    ImGui::Separator();

    // Property Editing
    if (!lights.empty() && state.selectedLightIdx < (u32)lights.size())
    {
        LightUBO& L = lights[state.selectedLightIdx];
        ImGui::SeparatorText("Properties");

        ImGui::ColorEdit3("Color", &L.color.x);
        ImGui::DragFloat("Intensity", &L.intensity, 0.5f, 0.0f, 1000.0f);

        if (L.type != LightType::Directional)
            ImGui::DragFloat3("Position", &L.position.x, 0.1f);

        if (L.type != LightType::Point)
        {
            if (ImGui::DragFloat3("Direction", &L.direction.x, 0.01f))
                L.direction = glm::normalize(L.direction);
        }

        if (ImGui::Button("Delete Selected"))
        {
            lights.erase(lights.begin() + state.selectedLightIdx);
            if (state.selectedLightIdx >= (u32)lights.size() && !lights.empty())
                state.selectedLightIdx = (u32)lights.size() - 1;
        }
    }
}