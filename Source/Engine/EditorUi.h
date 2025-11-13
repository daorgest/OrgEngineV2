//
// Created by Orgest on 7/7/2025.
//

#pragma once
#include "Application.h"
#include "VulkanInit.h"

struct DebugUBO;
struct Extent2D;
struct LightUBO;
struct SceneStats;
struct Camera;

struct State
{
    Platform::WindowContext* wc = nullptr;
    Renderer::VulkanDevice* device = nullptr;
    Renderer::VulkanSwapchain* swapchain = nullptr;
    DebugRenderer* debugRenderer = nullptr;
    DebugUBO* debugData = nullptr;
    SceneStats* sceneStats = nullptr;
    Vector<LightUBO>* lights;
    Camera* activeCamera = nullptr;
    // Panels (ImGui toggles)
    bool showMenuBar = true;
    bool showMainOverlay = true;
    bool showGPUInfo = false;
    bool showInputDebug = false;
    bool showLightEditor = true;
    bool showCameraProps = false;

    // Per-frame UI state
    f32 menuBarHeight = 0.f;
    f32 overlayAlpha = 1.f;
    f32 cameraSpeed = 0.0f;
};

struct EditorUI
{
    State state;

    static void InitEditorStyles();
    static bool Init(const Renderer::VulkanInstance* instance, const Renderer::VulkanDevice* device,
                     Renderer::VulkanSwapchain* swapchain);
    static void Destroy();
    static void BeginFrame();
    static void EndFrame();
    static void Render(VkCommandBuffer cmd);

    // Draws
    static void DrawCameraGizmo(const Camera* camera);
    static void DrawCameraProperties(Camera& camera);
    static void HoverToolTip(const char* tooltip);

    void DrawCameraSpeedPopup(f32 camSpeedPopupTime);

    // Menu bar with File, View, etc. Returns true if application should exit
    bool DrawMainMenuBar();

    // Main overlay window with stats, GPU info, etc.
    void DrawMainOverlay();
    static void DrawInputDebugPanel();

    // Light editor with gizmos (translate/rotate) using camera view/projection
    void DrawLightEditor() const;
};