//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <memory>
#include <string>

#include "RendererTypes.h"
#include "../../PrimTypes.h"

// for future audio
namespace Audio
{
    struct AudioState;

    struct SystemDebugInfo
    {
        u32 version = 0;
        std::string versionString;
        float cpuStudioUpdate = 0.0f;
        float cpuCoreDsp = 0.0f;

        int channelsPlaying = 0;
        float currentMemoryMB = 0.0f;
    };

    class System
    {
    public:
        System();
        ~System();

        System(const System&) = delete;
        System& operator=(const System&) = delete;

        Result<void> Init();
        void Update() const;

        // Studio Concepts
        void LoadBank(const std::string& filepath) const;
        void UnloadAllBanks() const;

        void PlayOneShot(const std::string& eventPath) const;

        void SetMasterVolume(f32 volume) const;
        void StopAll() const;

        SystemDebugInfo& GetSystemDebugInfo();

    private:
        std::unique_ptr<AudioState> mState;
        SystemDebugInfo mDebugInfo;
    };
}