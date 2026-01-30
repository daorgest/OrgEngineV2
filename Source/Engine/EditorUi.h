//
// Created by Orgest on 7/7/2025.
//

#pragma once
#include <span>

#include "RenderInterface.h"
#include "glm/vec3.hpp"
#include "Tools/Vector.h"

struct CameraComponent;

namespace Platform
{
    struct WindowContext;
}

class DebugRenderer;
struct DebugUBO;
struct LightUBO;
struct SceneStats;

struct State
{
    Platform::WindowContext* wc = nullptr;
    Renderer::GPUDevice* device = nullptr;
    Renderer::GPUSwapchain* swapchain = nullptr;
    DebugRenderer* debugRenderer = nullptr;
    DebugUBO* debugData = nullptr;
    SceneStats* sceneStats = nullptr;
    Vector<LightUBO>* lights;
    std::span<CameraComponent> cameraComponents;
    bool* freezeFrustum = nullptr;
    CameraComponent* frozenCam = nullptr;
    f32* aspectRatio = nullptr;

    u32 activeCameraIdx = 0;
    u32 selectedCameraIdx = 0;
    u32 selectedLightIdx = 0;
    // Panels (ImGui toggles)
    bool showMenuBar = true;
    bool showMainOverlay = true;
    bool showGPUInfo = false;
    bool followCamera = false;
    bool showEditorTools = true;
    bool showAboutPopup = false;
    bool noUI = false;

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

    static void InitEditorStyles();
    static bool Init(Renderer::GPUInterface* instance, Renderer::GPUDevice* device, Renderer::GPUSwapchain* swapchain);
    static void Destroy();
    static void BeginFrame();
    static void EndFrame();
    static void Render(Renderer::GPUCommandBuffer* cmd);

    // Draws
    static void DrawCameraGizmo(CameraComponent& camComp);
    void DrawCameraEditor();
    static void DrawCameraProperties(CameraComponent& camComp);
    void DrawCameraSpeedPopup(f32 camSpeedPopupTime) const;
    bool DrawMainMenuBar();
    void DrawMainOverlay() const;
    void AppInfoPopup();

    void DrawEditorTools();

    void UpdateLights(f32 deltaTime);
    void DrawLightGizmos(i32 selectedIdx, const CameraComponent& activeCam) const;
    void DrawLightEditor();

    // Helpers
    static void UpdateAlphaLerp(f32& currentAlpha, f32 minAlpha, f32 maxAlpha, f32 speed);
    static void HoverToolTip(const char* tooltip);
    static void ClampWindowToViewport();

};