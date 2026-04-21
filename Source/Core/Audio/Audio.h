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

    enum class AudioStatus : u8
    {
        None = 0,
        IsStreaming = 1 << 0,
        IsLooping = 1 << 1
    };

    enum class AudioOpenState : u8
    {
        Loading,
        Ready,
        Error,
        Connecting,
        Unknown
    };

    struct SystemDebugInfo
    {
        u32 version = 0;
        std::string versionString;

        // Granular CPU breakdown
        float cpuDsp = 0.0f;
        float cpuStream = 0.0f;
        float cpuGeometry = 0.0f;
        float cpuUpdate = 0.0f;
        float cpuTotal = 0.0f;

        // Resource stats
        int channelsPlaying = 0;
        int realChannelsPlaying = 0;
        float currentMemoryMB = 0.0f;
        float maxMemoryMB = 0.0f;
    };

    struct SoundDebugInfo
    {
        std::string name;
        std::string path;
        u32 lengthMs;
        u32 loadProgress;
        AudioOpenState state;
        bool isStreaming;
        bool isLooping;
        bool isStarving; // True if disk I/O can't keep up
        bool isDiskBusy; // True if the background thread is currently hitting the disk
    };

    inline AudioStatus operator|(AudioStatus a, AudioStatus b)
    {
        return static_cast<AudioStatus>(static_cast<u8>(a) | static_cast<u8>(b));
    }

    inline bool operator&(AudioStatus a, AudioStatus b)
    {
        return (static_cast<u8>(a) & static_cast<u8>(b)) != 0;
    }

    struct SoundAsset;

    class System
    {
    public:
        System();
        ~System();

        System(const System&) = delete;
        System& operator=(const System&) = delete;

        Result<void> Init();
        void SetMasterVolume(f32 volume) const;
        void StopAll() const;
        void LoadSound(const std::string& name, const std::string& filepath, AudioStatus mode = AudioStatus::IsLooping) const;
        void PlayAudio(const std::string& name) const;
        void TogglePause(const std::string& name) const;
        void UnloadSound(const std::string& name) const;
        void StopSound(const std::string& name) const;
        void Update() const;

        SystemDebugInfo& GetSystemDebugInfo();
        Vector<SoundDebugInfo> GetDebugInfo() const;
    private:
        std::unique_ptr<AudioState> mState;
        SystemDebugInfo mDebugInfo;
    };
}