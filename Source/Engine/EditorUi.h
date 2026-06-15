//
// Created by Orgest on 7/7/2025.
//

#pragma once
#include "RenderInterface.h"
#include "glm/vec3.hpp"
#include "Tools/Vector.h"

struct CameraComponent;

namespace Platform
{
    struct WindowContext;
}

class DebugRenderer;
struct SceneStats;

struct State
{
    Platform::WindowContext* wc = nullptr;
    Renderer::GPUDevice* device = nullptr;
    Renderer::GPUSwapchain* swapchain = nullptr;
    DebugRenderer* debugRenderer = nullptr;
    Engine::DebugUBO* debugData = nullptr;
    SceneStats* sceneStats = nullptr;
    Vector<Engine::LightUBO>* lights;
    Span<CameraComponent> cameraComponents;
    bool* freezeFrustum = nullptr;
    CameraComponent* frozenCam = nullptr;
    f32* aspectRatio = nullptr;
    bool pendingMSAAChange = false;

    u32 activeCameraIdx = 0;
    u32 selectedCameraIdx = 0;
    u32 selectedLightIdx = 0;
    u32 activeFlashlightIdx = (u32) - 1;
    f32 uiRenderScale = 1.0f;

    i32 uiSelectedMonitorIdx = 0;
    bool useCustomResolution = false;
    Platform::AspectRatio uiSelectedRatio = {16, 9};
    Renderer::Extent2D uiTargetExtent;
    u32 uiTargetRefreshRate = 0;
    Renderer::PresentMode uiSelectedVsyncMode = Renderer::PresentMode::VSyncOn;

    bool showDisplaySettings = true;

    // Panels (ImGui toggles)
    bool showMenuBar = true;
    bool showMainOverlay = true;
    bool showGPUInfo = false;
    bool showEditorTools = true;
    bool showAboutPopup = false;
    bool noUI = false;
    bool autoReloadShaders = false;
    bool pendingManualReload = false;

    // Per-frame UI state
    f32 menuBarHeight = 0.f;
    f32 overlayAlpha = 0.7f;
    f32 editorAlpha = 0.7f;
    f32 cameraSpeed = 0.0f;

    bool spinLights = false;
    f32 spinSpeed = 0.1f;
    f32 spinRadius = 12.0f;
    f32 spinHeight = 5.0f;
    glm::vec3 spinCenter = {0.0f, 0.0f, -10.0f};
    f32 currentLightTime = 0.0f;
};

struct EditorUI
{
    State state;

    static void InitEditorStyles(f32 dpiScale);
    bool Init(Renderer::GPUInterface* instance, Renderer::GPUDevice* device, Renderer::GPUSwapchain* swapchain);
    static void Destroy();
    static void BeginFrame();
    static void EndFrame();
    static void Render(Renderer::GPUCommandBuffer* cmd);

    // Draws
    static void DrawCameraGizmo(CameraComponent& camComp);
    void DrawCameraEditor();
    static void DrawCameraProperties(CameraComponent& camComp);
    void DrawCameraSpeedPopup(f32 camSpeedPopupTime) const;
    static void DrawDebugViewPopup(f32 debugViewPopupTime, Renderer::DebugView currentView);
    bool DrawMainMenuBar();
    void DrawMainOverlay() const;
    void AppInfoPopup();
    void DrawDisplaySettings();
    void DrawEditorTools();

    void UpdateLights() const;
    void DrawLightGizmos(i32 selectedIdx, const CameraComponent& activeCam) const;
    void DrawLightEditor();

    // Helpers
    static void UpdateAlphaLerp(f32& currentAlpha, f32 minAlpha, f32 maxAlpha, f32 speed = 12.0f);
    static void HoverToolTip(const char* tooltip);
    static void ClampWindowToViewport();
};