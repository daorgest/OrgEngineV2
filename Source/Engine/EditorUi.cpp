//
// Created by Orgest on 7/7/2025.
//

#include "EditorUi.h"

#include <imoguizmo.hpp>
#include <ufbx.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include "Application.h"
#include "Camera.h"
#include "DebugRenderer.h"
#include "MathFuncs.h"
#include "MeshStats.h"
#include "RendererTypes.h"
#include "VulkanConvert.h"
#ifndef IMGUI_DISABLE
#define IMGUI_ENABLE_FREETYPE
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#if ENGINE_PLATFORM_WIN32
#include <windows.h>
#include <backends/imgui_impl_win32.h>
#elif ENGINE_PLATFORM_SDL
#include <SDL3/SDL_version.h>
#include <backends/imgui_impl_sdl3.h>
#include "SDL3/SDL_vulkan.h"
#endif

#endif

static ImVec4 ToLinear(const ImVec4 col)
{
    // Approximation: Gamma 2.2 curve
    return ImVec4(
        std::pow(col.x, 2.2f),
        std::pow(col.y, 2.2f),
        std::pow(col.z, 2.2f),
        col.w // Alpha stays linear
    );
}

void EditorUI::InitEditorStyles(f32 dpiScale)
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
    colors[ImGuiCol_TableRowBg]    = ImVec4(mantle.x, mantle.y, mantle.z, 0.4f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(surface0.x, surface0.y, surface0.z, 0.5f);
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

    style.ScaleAllSizes(dpiScale);
}

// For multi-viewport support
#if ENGINE_PLATFORM_WIN32
static i32 CreateVulkanSurfaceForImGui(ImGuiViewport* vp, ImU64 vkInst, const void* vkAlloc = nullptr,
                                       ImU64* outVkSurface = nullptr)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = static_cast<HWND>(vp->PlatformHandleRaw);
    createInfo.hinstance = ::GetModuleHandle(nullptr);

    return vkCreateWin32SurfaceKHR(reinterpret_cast<VkInstance>(vkInst), &createInfo,
                                   static_cast<const VkAllocationCallbacks*>(vkAlloc),
                                   reinterpret_cast<VkSurfaceKHR*>(outVkSurface));
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

    f32 dpiScale = 1.0f;
#if ENGINE_PLATFORM_WIN32
    dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(swapchain->GetWindowHandle());
#elif ENGINE_PLATFORM_SDL
    dpiScale = SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(swapchain->GetWindowHandle()));
#endif

    InitEditorStyles(dpiScale);

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

    constexpr f32 fontSize = 18.0f;
    // we're going to rip this out from windows
    io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\segoeui.ttf)", fontSize * dpiScale, &cfg);

    // Initialize State with current swapchain values
    state.uiTargetExtent = swapchain->GetExtent();
    state.uiSelectedVsyncMode = swapchain->GetPresentMode();
    state.uiRenderScale = swapchain->GetRenderScale();

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
    const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 1000.0f, 0.1f);


    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    const f32 fontSize = ImGui::GetFontSize();
    const f32 gizmoSize = 7.0f * fontSize;
    const f32 padding = 0.55f * fontSize;

    // Position in top-right corner of work area
    const f32 xPos = work_pos.x + work_size.x - gizmoSize - padding;
    const f32 yPos = work_pos.y + padding;

    // Draw directly using viewport foreground draw list (sticks to main viewport)
    ImOGuizmo::SetRect(xPos, yPos, gizmoSize);
    ImOGuizmo::SetDrawList(ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(viewport)));
    ImOGuizmo::BeginFrame();
    glm::mat4 viewCopy = camComp.camera.view;

    ImOGuizmo::DrawGizmo(glm::value_ptr(viewCopy), glm::value_ptr(proj), 5.0f);

    if (viewCopy != camComp.camera.view)
    {
        camComp.camera.view = viewCopy;

        // Extract World Position
        glm::mat4 invView = glm::inverse(viewCopy);
        camComp.position = glm::vec3(invView[3]);

        // Extract Forward Vector to rebuild Pitch and Yaw
        const glm::vec3 fwd = -glm::vec3(invView[2]);
        camComp.controller.pitch = glm::degrees(std::asin(fwd.y));
        camComp.controller.yaw = glm::degrees(std::atan2(fwd.x, -fwd.z));

        // Rebuild rotation quaternion safely
        const glm::quat qYaw = glm::angleAxis(glm::radians(camComp.controller.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat qPitch = glm::angleAxis(glm::radians(camComp.controller.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        camComp.camera.rotation = qYaw * qPitch;
    }
}

void EditorUI::DrawCameraEditor()
{
    static ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    const float labelWidth = 90.0f;

    // --- Section 1: Frustum Debugger ---
    if (state.freezeFrustum && state.frozenCam)
    {
        ImGui::SeparatorText("FRUSTUM DEBUGGER");

        bool isFrozen = *state.freezeFrustum;
        if (ImGui::Checkbox("Freeze Frustum (F7)", &isFrozen))
        {
            *state.freezeFrustum = isFrozen;
            if (isFrozen) *state.frozenCam = state.cameraComponents[state.activeCameraIdx];
        }

        if (isFrozen)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.98f, 0.44f, 0.49f, 1.0f), " [EDITING GHOST]");

            CameraComponent* ghost = state.frozenCam;
            bool changed = false;

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));

            if (ImGui::BeginTable("##GhostTable", 2, tableFlags))
            {
                ImGui::TableSetupColumn("Prop", ImGuiTableColumnFlags_WidthFixed, labelWidth);
                ImGui::TableSetupColumn("Val",  ImGuiTableColumnFlags_WidthStretch);

                // Position
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Ghost Pos");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::DragFloat3("##GPos", glm::value_ptr(ghost->position), 0.1f)) changed = true;

                // Orientation (Now editing the controller's isolated Euler angles)
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Ghost Rot");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
                if (ImGui::DragFloat("##GYaw", &ghost->controller.yaw, 0.5f, -180.0f, 180.0f, "Y: %.1f")) changed = true;
                ImGui::SameLine();
                if (ImGui::DragFloat("##GPitch", &ghost->controller.pitch, 0.5f, -89.0f, 89.0f, "P: %.1f")) changed = true;
                ImGui::PopItemWidth();

                // FOV
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Ghost FOV");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SliderFloat("##GFOV", &ghost->camera.fov, 10.0f, 160.0f, "%.0f deg")) changed = true;

                ImGui::EndTable();
            }
            ImGui::PopStyleColor();

            if (changed)
            {
                const ImGuiViewport* vp = ImGui::GetMainViewport();

                // Rebuild the quaternion safely without gimbal lock from the UI sliders
                glm::quat qYaw = glm::angleAxis(Radians(ghost->controller.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::quat qPitch = glm::angleAxis(Radians(ghost->controller.pitch), glm::vec3(1.0f, 0.0f, 0.0f));
                ghost->camera.rotation = qYaw * qPitch;

                ghost->camera.Update(ghost->position, vp->WorkSize.x / vp->WorkSize.y);
            }

            if (ImGui::Button("Teleport Real Cam to Ghost", ImVec2(-FLT_MIN, 0)))
            {
                CameraComponent& active = state.cameraComponents[state.activeCameraIdx];
                active.position = ghost->position;

                // Sync the controller state AND the quaternion
                active.controller.yaw = ghost->controller.yaw;
                active.controller.pitch = ghost->controller.pitch;
                active.camera.rotation = ghost->camera.rotation;
                active.camera.fov = ghost->camera.fov;

                const ImGuiViewport* vp = ImGui::GetMainViewport();
                active.camera.Update(active.position, vp->WorkSize.x / vp->WorkSize.y);
            }
        }
        ImGui::Spacing();
    }

    const CameraComponent& active = state.cameraComponents[state.activeCameraIdx];
    ImGui::SeparatorText("Controller State (Live)");

    const FPSCamera& ctrl = active.controller;

    // A quick lambda to draw disabled (read-only) checkboxes
    auto DrawFlagUI = [](const char* label, bool isActive)
    {
        ImGui::BeginDisabled();
        bool val = isActive;
        ImGui::Checkbox(label, &val);
        ImGui::EndDisabled();
    };

    if (ImGui::BeginTable("FlagsTable", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawFlagUI("Moving", HasAny(ctrl.flags, FPSFlags::Moving));

        ImGui::TableSetColumnIndex(1);
        DrawFlagUI("Sprinting", HasAny(ctrl.flags, FPSFlags::Sprinting));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawFlagUI("Crouching", HasAny(ctrl.flags, FPSFlags::Crouching));

        ImGui::TableSetColumnIndex(1);
        DrawFlagUI("Airborne", HasAny(ctrl.flags, FPSFlags::Airborne));

        ImGui::EndTable();
    }

    // --- Section 2: Camera List ---
    ImGui::SeparatorText("SCENE CAMERAS");
    if (ImGui::BeginListBox("##SceneCameras", ImVec2(-FLT_MIN, 7 * ImGui::GetFrameHeightWithSpacing())))
    {
        for (u32 i = 0; i < state.cameraComponents.size(); ++i)
        {
            const bool isActive = (i == state.activeCameraIdx);
            const bool isSelected = (i == state.selectedCameraIdx);

            char label[64];
            std::snprintf(label, sizeof(label), "%s Camera %u %s",
                            isActive ? "[ACTIVE]" : "       ",
                            i,
                            isSelected ? "(Selected)" : "");

            if (ImGui::Selectable(label, isSelected)) state.selectedCameraIdx = i;
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) state.activeCameraIdx = i;
        }
        ImGui::EndListBox();
    }

    ImGui::Spacing();

    // --- Section 3: Selected Actions ---
    const u32 selIdx = state.selectedCameraIdx;
    if (selIdx < state.cameraComponents.size())
    {
        CameraComponent& targetComp = state.cameraComponents[selIdx];

        if (ImGui::Button("Possess", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 4.0f, 0))) state.activeCameraIdx = selIdx;
        ImGui::SameLine();
        if (ImGui::Button("Snap to Me", ImVec2(-FLT_MIN, 0)))
        {
            const CameraComponent& activeComp = state.cameraComponents[state.activeCameraIdx];
            targetComp.position = activeComp.position;

            // Sync controller state AND quaternion
            targetComp.controller.yaw = activeComp.controller.yaw;
            targetComp.controller.pitch = activeComp.controller.pitch;
            targetComp.camera.rotation = activeComp.camera.rotation;

            const ImGuiViewport* vp = ImGui::GetMainViewport();
            targetComp.camera.Update(targetComp.position, vp->WorkSize.x / vp->WorkSize.y);
        }

        DrawCameraProperties(targetComp);
    }
}

void EditorUI::DrawCameraProperties(CameraComponent& camComp)
{
    ImGui::SeparatorText("Selected Camera Settings");

    if (ImGui::BeginTable("CameraProps", 2, ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 6.0f * ImGui::GetFontSize());
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
        ImGui::SliderFloat("##CameraFOV", &camComp.camera.fov, 5.0f, 160.0f);

        // --- Clipping Planes ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Near/Far");
        ImGui::TableSetColumnIndex(1);
        ImGui::InputFloat("Near", &camComp.camera.nearPlane);
        ImGui::InputFloat("Far", &camComp.camera.farPlane);

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
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    // Keep window within viewport bounds
    if (size.x <= work_size.x)
        pos.x = std::clamp(pos.x, work_pos.x, work_pos.x + work_size.x - size.x);
    else
        pos.x = work_pos.x;

    if (size.y <= work_size.y)
        pos.y = std::clamp(pos.y, work_pos.y, work_pos.y + work_size.y - size.y);
    else
        pos.y = work_pos.y;

    ImGui::SetWindowPos(pos);
}

void EditorUI::DrawCameraSpeedPopup(f32 camSpeedPopupTime) const
{
    if (camSpeedPopupTime <= 0.0f) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    const ImVec2 pos = {work_pos.x + (work_size.x * 0.5f), work_pos.y + (work_size.y * 0.12f)};

    const f32 alpha = camSpeedPopupTime / 1.5f;

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

void EditorUI::DrawDebugViewPopup(f32 debugViewPopupTime, Renderer::DebugView currentView)
{
    if (debugViewPopupTime <= 0.0f) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    const f32 fontSize = ImGui::GetFontSize();

    // Position in center of work area (slightly higher than center: 40% down)
    const ImVec2 pos = {work_pos.x + (work_size.x * 0.5f), work_pos.y + (work_size.y * 0.20f)};

    // Fade out over the last 0.5 seconds
    const f32 alpha = std::min(1.0f, debugViewPopupTime / 0.5f);

    ImGui::SetNextWindowBgAlpha(alpha * 0.7f); // Slightly transparent background
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.45f * fontSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.1f * fontSize, 0.55f * fontSize));

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
            ImGui::MenuItem("Show Main Overlay", nullptr, &state.showMainOverlay);
            ImGui::MenuItem("Show Menu Bar", "F3", &state.showMenuBar);
            ImGui::MenuItem("Show GPU Info", "F4", &state.showGPUInfo);
            ImGui::MenuItem("Show Editor Tools", "T", &state.showEditorTools);

            if (ImGui::BeginMenu("Render Views"))
            {
                for (const auto& [value, label] : Renderer::kDebugViews)
                {
                    bool selected = (state.debugData->debugMode == value);
                    if (ImGui::MenuItem(label, nullptr, selected)) state.debugData->debugMode = value;
                    if (selected) ImGui::SetItemDefaultFocus();
                }

                ImGui::EndMenu();
            }

            // In EditorUi.cpp -> DrawMainMenuBar()
            if (ImGui::BeginMenu("MSAA"))
            {
                const Renderer::SampleCount maxSupported = state.device->GetMaxUsableSampleCount();

                for (const auto& mode : Renderer::kMSAAModes)
                {
                    if (mode.count > maxSupported) continue;

                    const bool isSelected = (state.device->currentSamples == mode.count);
                    if (ImGui::MenuItem(mode.label, nullptr, isSelected))
                    {
                        state.device->currentSamples = mode.count;
                        state.pendingMSAAChange = true;
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("VSync"))
            {
                for (const auto& [mode, label] : Renderer::kVsyncModes)
                {
                    const bool selected = (state.swapchain->GetPresentMode() == mode);
                    if (ImGui::MenuItem(label, nullptr, selected))
                    {
                        state.swapchain->SetVsyncMode(mode);
#ifdef VULKAN_BUILD
                        ImGui_ImplVulkan_SetMinImageCount(MAX_FRAME_OVERLAP);
#endif
                    }
                    if (selected) { ImGui::SetItemDefaultFocus(); }
                }
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
    const bool hovered = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_ChildWindows |
        ImGuiHoveredFlags_AllowWhenBlockedByPopup |
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
    );

    const f32 target = hovered ? maxAlpha : minAlpha;

    if (std::abs(currentAlpha - target) > 0.001f)
    {
        const f32 t = 1.0f - std::exp(-speed * io.DeltaTime);
        currentAlpha = std::lerp(currentAlpha, target, t);
    }
    else
    {
        currentAlpha = target;
    }
}

void EditorUI::DrawMainOverlay() const
{
	if (!state.showMainOverlay) return;
    using namespace ImGui;

    const bool showDepthRange = state.debugData->debugMode == Renderer::DebugView::Depth;
    enum Corner { Custom = -1, TopLeft, TopRight, BottomLeft, BottomRight, Center };
    static i32 corner = TopLeft;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 work_pos = vp->WorkPos;
    const ImVec2 work_size = vp->WorkSize;

    const f32 fontSize = ImGui::GetFontSize();
    const f32 padding = std::clamp(0.5f * fontSize, 8.0f, 24.0f);

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
    auto min_size = ImVec2(12.0f * fontSize, 5.5f * fontSize);

    if (showDepthRange)
        min_size.x = std::max(min_size.x, 18.0f * fontSize); // ~320px scaled
    if (state.debugRenderer->enabled)
        min_size.x = std::max(min_size.x, 23.5f * fontSize); // ~420px scaled

    // Expand height for extra rows
    f32 extra_rows = 0.0f;
    if (showDepthRange) extra_rows += 1.0f;
    if (state.debugRenderer->enabled) extra_rows += 2.0f;
    min_size.y += extra_rows * ImGui::GetFrameHeightWithSpacing();
    SetNextWindowSizeConstraints(min_size, ImVec2(FLT_MAX, FLT_MAX));

    SetNextWindowBgAlpha(state.overlayAlpha);
    PushStyleVar(ImGuiStyleVar_Alpha, state.overlayAlpha);

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
        UpdateAlphaLerp(const_cast<f32&>(state.overlayAlpha), 0.6f, 1.0f, 12.0f);
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
                TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 7.0f * fontSize);
                TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

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
                TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 7.0f * fontSize);
                TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                // 1. Native Window Resolution
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Native Res");
                ImGui::TableSetColumnIndex(1);
                const Renderer::Extent2D nativeRes = state.swapchain->GetExtent();
                ImGui::Text("%u x %u", nativeRes.width, nativeRes.height);
                EditorUI::HoverToolTip("Physical window and UI presentation resolution.");

                // 2. Internal Scene Render Resolution
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Render Res");
                ImGui::TableSetColumnIndex(1);
                const Renderer::Extent2D renderRes = state.swapchain->GetRenderExtent();
                ImGui::Text("%u x %u", renderRes.width, renderRes.height);
                EditorUI::HoverToolTip("Internal 3D scene rendering resolution before upscaling.");

                // 3. Current Scale Multiplier
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Scale");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f x", state.swapchain->GetRenderScale());
                EditorUI::HoverToolTip("Current render scale multiplier driving the Render Res.");

                ImGui::EndTable();
            }
        }

        ImGui::SeparatorText("Mesh / Input status");
        {
            if (ImGui::BeginTable("SceneStatsTable", 2, ImGuiTableFlags_SizingStretchProp))
            {
                TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 7.0f * fontSize);
                TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

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
                TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 7.0f * fontSize);
                TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

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
                ImGui::Text("%.1f GB", static_cast<double>(state.device->GetDeviceDesc().dedicatedVideoMemory) / 1_GiB);

                ImGui::EndTable();
            }
        }

        // Dynamic tables from toggles
        if (ImGui::BeginTable("Toggles", 2, ImGuiTableFlags_SizingStretchProp))
        {
            TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 7.0f * fontSize);
            TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

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
                ImGui::SeparatorText("Debug Line Settings");
                ImGui::TableSetColumnIndex(1);
                ImGui::Dummy(ImVec2(0, 0));

                // Depth bias slider remains active for Z-fighting adjustments
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Depth Bias");
                ImGui::TableSetColumnIndex(1);
                static f32 depthBias = 0.0f;
                if (ImGui::SliderFloat("##AABBBias", &depthBias, 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic))
                {
                    state.debugRenderer->SetDepthBias(depthBias);
                }

                // Reset option handles bias values only
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Reset Bias", ImVec2(-FLT_MIN, 0)))
                {
                    depthBias = 0.0f;
                    state.debugRenderer->SetDepthBias(depthBias);
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
                TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 7.0f * fontSize);
                TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Disable Normal Map");
                ImGui::TableSetColumnIndex(1);
                {
                    bool dn = state.debugData->disableNormalMap;
                	if (ImGui::Checkbox("##DisableNormal", &dn))
                    {
                        state.debugData->disableNormalMap = dn ? 1u : 0u;
                    }
                    state.debugData->disableNormalMap = dn ? 1u : 0u;
                    if (dn != 1)
                    {
                        ImGui::SliderFloat("Normal Map Strength", &state.debugData->normalStrength, 0.0f, 2.0f);
                    }
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

            // Add these to the table or directly below it
            ImGui::SeparatorText("PBR Aesthetics Tuning");

            // Ambient Strength Slider
            ImGui::SliderFloat("Ambient Intensity", &state.debugData->ambientStrength, 0.0f, 0.5f, "%.2f");
            EditorUI::HoverToolTip("Controls the base ambient light level (darkens shadows).");

            // AO Strength Slider
            ImGui::SliderFloat("AO Intensity", &state.debugData->aoStrength, 0.0f, 2.0f, "%.2f");
            EditorUI::HoverToolTip("Multiplies the AO effect; high values deepen crevices.");

            // IBL Roughness Bias Slider
            ImGui::SliderFloat("IBL Mip Bias", &state.debugData->iblRoughnessMipBias, 0.0f, 1.0f, "%.2f");
            EditorUI::HoverToolTip("Adjusts the blurriness of reflections; 0 is sharp, 1 is mirror-diffuse.");

            // Reflectivity Scales
            ImGui::SliderFloat("Metallic Reflect Scale", &state.debugData->metallicReflectScale, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Roughness Reflect Scale", &state.debugData->roughnessReflectScale, 0.0f, 1.0f, "%.2f");

            // Shadow Tinting
            ImGui::Spacing();
            ImGui::TextDisabled("Shadow Aesthetics");
            ImGui::ColorEdit3("Shadow Tint", &state.debugData->shadowTint.x, ImGuiColorEditFlags_Float);
            EditorUI::HoverToolTip("The color of shadowed areas (lerps from this color to Albedo)");
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

    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

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
                 (VMA_VERSION) >> 22U,
                 ((VMA_VERSION) >> 12U) & 0x3FFU,
                 (VMA_VERSION) & 0xFFFU);
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

        constexpr u32 uv = UFBX_HEADER_VERSION;
        snprintf(versionBuffer, sizeof(versionBuffer), "%u.%u.%u",
                 uv / 10000, (uv / 100) % 100, uv % 100);
        AddLibRow("ufbx", versionBuffer);

        ImGui::EndTable();
    }

    ImGui::End();
}

static std::string GetCleanMonitorName(const std::string& rawName, i32 index)
{
    if (rawName.empty())
    {
        return fmt::format("Display {}", index + 1);
    }

    // Win32 device path parser fallback (e.g., "\\.\DISPLAY1" -> "Display 1")
    const size_t pos = rawName.find("DISPLAY");
    if (pos != std::string::npos)
    {
        return "Display " + rawName.substr(pos + 7);
    }


    return rawName;
}

void EditorUI::DrawDisplaySettings()
{
    if (!state.showDisplaySettings || !state.wc) return;

    auto& wc = *state.wc;
    auto& settings = wc.displaySettings;
    auto* sc = static_cast<Renderer::VulkanSwapchain*>(state.swapchain);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 center = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // Dynamic DPI-aware width
    const f32 minWidth = 30.0f * ImGui::GetFontSize();
    ImGui::SetNextWindowSizeConstraints(ImVec2(minWidth, -1), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::Begin("Display Settings", &state.showDisplaySettings, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (state.uiSelectedMonitorIdx >= settings.monitors.size())
            state.uiSelectedMonitorIdx = wc.activeMonitorIndex;

        const auto& currentMonitor = settings.monitors[state.uiSelectedMonitorIdx];
        std::string currentMonitorName = GetCleanMonitorName(currentMonitor.name, state.uiSelectedMonitorIdx);

        // =========================================================
        ImGui::SeparatorText("Display");

        // Use a table for the perfect Left Label / Right Control alignment
        if (ImGui::BeginTable("DisplaySettingsTable", 2, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Display");
            ImGui::TableSetColumnIndex(1);

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##Display", currentMonitorName.c_str()))
            {
                for (i32 i = 0; i < settings.monitors.size(); ++i)
                {
                    std::string itemName = GetCleanMonitorName(settings.monitors[i].name, i);
                    const bool isSelected = (state.uiSelectedMonitorIdx == i);
                    if (ImGui::Selectable(itemName.c_str(), isSelected))
                    {
                        state.uiSelectedMonitorIdx = i;
                        state.uiTargetExtent = {
                            settings.monitors[i].desktopMode.width, settings.monitors[i].desktopMode.height
                        };
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // --- 2. Custom Resolution Toggle ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); // Empty left column for toggle
            ImGui::TableSetColumnIndex(1);
            ImGui::Checkbox("Custom resolution", &state.useCustomResolution);

            // -----------------------------------------------------
            // BEGIN DISABLED GROUP: Gray out if Custom is OFF
            // -----------------------------------------------------
            ImGui::BeginDisabled(!state.useCustomResolution);

            // --- 3. Aspect Ratio Radio Buttons ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Aspect ratio");
            ImGui::TableSetColumnIndex(1);

            auto RadioRatio = [&](const char* label, u32 w, u32 h)
            {
                bool active = (state.uiSelectedRatio.x == w && state.uiSelectedRatio.y == h);
                if (ImGui::RadioButton(label, active)) state.uiSelectedRatio = {w, h};
            };

            RadioRatio("16x9", 16, 9);
            ImGui::SameLine();
            RadioRatio("16x10", 16, 10);
            ImGui::SameLine();
            RadioRatio("21x9", 21, 9);

            // --- 4. Resolution Dropdown ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Resolution");
            ImGui::TableSetColumnIndex(1);

            // If Custom is off, force the target extent to be the monitor's native resolution
            if (!state.useCustomResolution)
            {
                state.uiTargetExtent = {currentMonitor.desktopMode.width, currentMonitor.desktopMode.height};
                state.uiTargetRefreshRate = currentMonitor.desktopMode.refreshRate;
            }

            if (state.uiTargetRefreshRate == 0) state.uiTargetRefreshRate = currentMonitor.desktopMode.refreshRate;

            char resPreview[64];
            std::snprintf(resPreview, sizeof(resPreview), "%u x %u (%u Hz)",
                          state.uiTargetExtent.width, state.uiTargetExtent.height, state.uiTargetRefreshRate);

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##Resolution", resPreview))
            {
                if (currentMonitor.supportedModes.contains(state.uiSelectedRatio))
                {
                    auto modes = currentMonitor.supportedModes.at(state.uiSelectedRatio);

                    // Sort descending: Largest width -> Largest height -> Highest Hz
                    std::ranges::sort(modes, [](const Platform::VideoMode& a, const Platform::VideoMode& b)
                    {
                        if (a.width != b.width) return a.width > b.width;
                        if (a.height != b.height) return a.height > b.height;
                        return a.refreshRate > b.refreshRate;
                    });

                    for (const auto& res : modes)
                    {
                        char resLabel[64];
                        std::snprintf(resLabel, sizeof(resLabel), "%u x %u (%u Hz)", res.width, res.height,
                                      res.refreshRate);

                        const bool isSelected = (state.uiTargetExtent.width == res.width &&
                            state.uiTargetExtent.height == res.height &&
                            state.uiTargetRefreshRate == res.refreshRate);

                        if (ImGui::Selectable(resLabel, isSelected))
                        {
                            state.uiTargetExtent = {res.width, res.height};
                            state.uiTargetRefreshRate = res.refreshRate;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                }
                else
                {
                    ImGui::Selectable("No modes for this ratio", false, ImGuiSelectableFlags_Disabled);
                }
                ImGui::EndCombo();
            }

            ImGui::EndDisabled();
            // -----------------------------------------------------
            // END DISABLED GROUP
            // -----------------------------------------------------

            // --- 5. Window Mode ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Window mode");
            ImGui::TableSetColumnIndex(1);

            const char* modeNames[] = {"Windowed", "Borderless Window"};
            i32 currentModeIdx = (wc.displayMode == Platform::DisplayMode::BorderlessFullscreen) ? 1 : 0;

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##WindowMode", &currentModeIdx, modeNames, 2))
            {
                wc.displayMode = (currentModeIdx == 1)
                                     ? Platform::DisplayMode::BorderlessFullscreen
                                     : Platform::DisplayMode::Windowed;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Render Scale");
            ImGui::TableSetColumnIndex(1);

            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##RenderScale", &state.uiRenderScale, 0.25f, 2.0f, "%.2f x");

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                sc->SetRenderScale(state.uiRenderScale);
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Apply Button ---
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.4f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.5f, 0.4f, 1.0f));
        if (ImGui::Button("Apply Settings", ImVec2(140.0f, 32.0f)))
        {
            wc.activeMonitorIndex = state.uiSelectedMonitorIdx;
            Platform::MoveWindowToMonitor(wc, wc.activeMonitorIndex);

            u32 physW = state.uiTargetExtent.width;
            u32 physH = state.uiTargetExtent.height;
            const u32 physHz = state.uiTargetRefreshRate;

            // If Borderless, the physical window MUST forcefully match the native desktop bounds
            if (wc.displayMode == Platform::DisplayMode::BorderlessFullscreen)
            {
                physW = currentMonitor.desktopMode.width;
                physH = currentMonitor.desktopMode.height;
            }

            // FIX: Only update the refresh rate
            Platform::SetMonitorRefreshRate(wc, physHz);
            Platform::SetDisplayMode(wc, wc.displayMode);
            Platform::SetWindowResolution(wc, physW, physH);

            sc->needsRecreation = true;
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
}

void EditorUI::DrawEditorTools()
{
    if (state.noUI) return;


    DrawLightGizmos(state.selectedLightIdx, state.cameraComponents[state.activeCameraIdx]);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const f32 fontSize = ImGui::GetFontSize();
    const f32 windowWidth = 22.5f * fontSize;
    const f32 gizmoOffset = 9.0f * fontSize;
    const f32 padding = 1.0f * fontSize;

    // Position: X is right-aligned minus window width and padding.
    // Y is top-aligned plus gizmo size and double padding.
    const f32 defaultX = viewport->WorkPos.x + viewport->WorkSize.x - windowWidth - padding;
    const f32 defaultY = viewport->WorkPos.y + padding + gizmoOffset + padding;

    ImGui::SetNextWindowPos(ImVec2(defaultX, defaultY), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(windowWidth, -1), ImVec2(windowWidth, FLT_MAX));

    ImGui::SetNextWindowBgAlpha(state.editorAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, state.editorAlpha);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Editor Tools", &state.showEditorTools, flags))
    {
        if (ImGui::IsWindowAppearing() || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            ClampWindowToViewport();
        }

        if (ImGui::BeginTabBar("EditorManagerTabs", ImGuiTabBarFlags_Reorderable))
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

            if (ImGui::BeginTabItem("Shaders"))
            {
                ImGui::Checkbox("Auto Reload Shaders", &state.autoReloadShaders);

                if (ImGui::Button("Reload Shaders Now", ImVec2(-FLT_MIN, 0)))
                {
                    state.pendingManualReload = true;
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        UpdateAlphaLerp(state.editorAlpha, 0.6f, 1.0f);
    }

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

using namespace Engine;

static LightUBO MakeDirectional()
{
    return {
        .position = {0.0f, 15.0f, 0.0f},
        .range = 0.0f,
        .direction = glm::normalize(glm::vec3{-0.5f, -1.f, -0.3f}),
        .innerCone = 0.0f,
        .color     = { 1.f, 0.95f, 0.85f },
        .intensity = 3.f,
        .type      = LightType::Directional,
        .outerCone = 0.0f
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

void EditorUI::UpdateLights() const
{
    if (!state.lights) return;
    auto& lights = *state.lights;
    const CameraComponent& activeCam = state.cameraComponents[state.activeCameraIdx];

    if (state.activeFlashlightIdx != (u32)-1 && state.activeFlashlightIdx < static_cast<u32>(lights.size()))
    {
        auto& L = lights[state.activeFlashlightIdx];
        if (L.type == LightType::Spot || L.type == LightType::Point)
        {
            L.position = activeCam.position;
            if (L.type == LightType::Spot)
                L.direction = glm::normalize(activeCam.camera.GetForward());
        }
    }
}

void EditorUI::DrawLightGizmos(const i32 selectedIdx, const CameraComponent& activeCam) const
{
    if (!state.lights) return;

    auto& lights = *state.lights;
    ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
    const ImGuiIO& io = ImGui::GetIO();


    const Camera& cam = activeCam.camera;

    for (i32 i = 0; i < static_cast<i32>(lights.size()); ++i)
    {
        ImVec2 sPos;
        if (!ProjectToScreen(lights[i].position, cam.view, cam.projection, sPos)) continue;

        // Viewport Interaction Logic
        constexpr f32 selectRadius = 22.0f;
        const f32 distSq = glm::length2(glm::vec2(io.MousePos.x - sPos.x, io.MousePos.y - sPos.y));

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse && distSq < (selectRadius * selectRadius))
        {
            const_cast<State&>(state).selectedLightIdx = static_cast<u32>(i);
        }

        const bool isSel = (i == selectedIdx);

        // Clang-Tidy: Mark local variables const if they don't change
        const ImU32 col = (lights[i].type == LightType::Directional)
                              ? IM_COL32(249, 226, 175, 255)
                              : (lights[i].type == LightType::Point)
                              ? IM_COL32(137, 180, 250, 255) :
                              IM_COL32(166, 227, 161, 255);

        if (isSel)
        {
            dl->AddCircle(sPos, 16.0f, col, 24, 3.5f); // Selection Glow
            dl->AddCircle(sPos, 19.0f, IM_COL32(255, 255, 255, 80), 24, 1.0f);

            if (lights[i].type == LightType::Directional)
            {
                ImVec2 sEnd;
                const glm::vec3 targetPos = lights[i].position + (lights[i].direction * 4.0f);
                if (ProjectToScreen(targetPos, cam.view, cam.projection, sEnd))
                {
                    dl->AddLine(sPos, sEnd, col, 2.5f);
                    dl->AddCircleFilled(sEnd, 4.0f, col);
                }
            }
        }

        dl->AddCircleFilled(sPos, isSel ? 8.0f : 5.0f, col);

        char idLabel[16];
        std::snprintf(idLabel, sizeof(idLabel), "L%d", i);
        // Added explicit float literals for coordinates to avoid implicit double-to-float conversions
        dl->AddText({sPos.x + 15.0f, sPos.y - 15.0f}, col, idLabel);
    }
}

void EditorUI::DrawLightEditor()
{
    // Ensure we handle potential nullptrs safely
    if (!state.lights) return;

    auto& lights = *state.lights;
    const CameraComponent& activeCam = state.cameraComponents[state.activeCameraIdx];

    // --- 1. Creation Controls ---
    if (ImGui::Button("+ Directional")) lights.push_back(MakeDirectional());
    ImGui::SameLine();
    if (ImGui::Button("+ Point")) lights.push_back(MakePoint(activeCam.camera.GetForward(), activeCam.position));
    ImGui::SameLine();
    if (ImGui::Button("+ Spot")) lights.push_back(MakeSpot(activeCam.camera.GetForward(), activeCam.position));

    ImGui::Separator();

    // --- 2. Listbox Selection ---
    if (ImGui::BeginListBox("##LightsList", ImVec2(-FLT_MIN, 8.0f * ImGui::GetFrameHeightWithSpacing())))
    {
        for (vecSizeType i = 0; i < lights.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));

            const bool isSelected = (state.selectedLightIdx == static_cast<u32>(i));
            const char* typeIcon = (lights[i].type == LightType::Directional)
                                       ? "[D]"
                                       : (lights[i].type == LightType::Point)
                                       ? "[P]"
                                       : "[S]";

            if (ImGui::Selectable(typeIcon, isSelected)) {
                state.selectedLightIdx = static_cast<u32>(i);
            }
            ImGui::SameLine();
            ImGui::Text("Light %zu", static_cast<size_t>(i));

            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();

    // --- 3. Global Settings ---
    if (ImGui::CollapsingHeader("Global Spin Settings", ImGuiTableFlags_None | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::Checkbox("Enable Animation", &state.spinLights);
        ImGui::DragFloat("Speed", &state.spinSpeed, 0.05f);
        ImGui::DragFloat("Radius", &state.spinRadius, 0.1f);
        ImGui::DragFloat("Height", &state.spinHeight, 0.1f);
        ImGui::DragFloat3("Center", &state.spinCenter.x, 0.1f);
    }

    ImGui::Separator();

    // --- 4. Property Editing ---
    if (!lights.empty() && state.selectedLightIdx < lights.size())
    {
        LightUBO& L = lights[state.selectedLightIdx];
        ImGui::SeparatorText("Properties");

        ImGui::ColorEdit3("Color", &L.color.r, ImGuiColorEditFlags_Float);
        ImGui::DragFloat("Intensity", &L.intensity, 0.5f, 0.0f, 1000.0f);

        if (L.type != LightType::Directional)
        {
            ImGui::DragFloat("Range", &L.range, 0.5f, 1.0f, 1000.0f);
            ImGui::DragFloat3("Position", &L.position.x, 0.1f);
        }
        else
        {
            ImGui::DragFloat3("Editor Handle Pos", &L.position.x, 0.1f);
        }

        if (L.type != LightType::Point)
            if (L.type != LightType::Point)
            {
                if (ImGui::DragFloat3("Direction", &L.direction.x, 0.01f))
                {
                    if (glm::length2(L.direction) > 0.0001f)
                    {
                        L.direction = glm::normalize(L.direction);
                    }
                }

                if (L.type == LightType::Directional)
                {
                    const float sunPitch = glm::degrees(std::asin(-L.direction.y));
                    const float sunYaw = glm::degrees(std::atan2(L.direction.x, -L.direction.z));
                    ImGui::TextDisabled("Sun Orientation Angle: Pitch %.1f° | Yaw %.1f°", sunPitch, sunYaw);
                }
            }

        if (ImGui::Button("Delete Selected"))
        {
            lights.erase(state.selectedLightIdx);

            if (lights.empty()) {
                state.selectedLightIdx = 0;
            } else if (state.selectedLightIdx >= lights.size()) {
                state.selectedLightIdx = static_cast<u32>(lights.size() - 1);
            }
        }
    }
}