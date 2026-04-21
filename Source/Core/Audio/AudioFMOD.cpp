//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <algorithm>

#include "Audio.h"
#include <fmod.hpp>
#include <fmod_errors.h>
#include <string>
#include <unordered_map>

#include "Tools/Logger.h"

template <>
struct fmt::formatter<FMOD_RESULT> : formatter<int>
{
    auto format(FMOD_RESULT result, format_context& ctx) const
    {
        // Forward the cast integer to the standard int formatter
        return formatter<int>::format(result, ctx);
    }
};

namespace Audio
{
    struct SoundAsset
    {
        FMOD::Sound* sound = nullptr;
        FMOD::Channel* channel = nullptr;
        std::string path;
        AudioStatus status;
        u32 lengthMs = 0;
    };

    struct AudioState
    {
        FMOD::System* coreSystem = nullptr;
        FMOD::ChannelGroup* masterGroup = nullptr;
        std::unordered_map<std::string, SoundAsset> soundCache;
    };

    System::System() : mState(std::make_unique<AudioState>()) {}

    System::~System()
    {
        if (mState->coreSystem)
        {
            mState->coreSystem->release();
        }
    }

    Result<void> System::Init()
    {
        FMOD_RESULT result = FMOD::System_Create(&mState->coreSystem);
        if (result != FMOD_OK)
        {
            return std::unexpected{ OrgErrCode::AudioInitFailed };
        }

        result = mState->coreSystem->init(512, FMOD_INIT_NORMAL, nullptr);
        if (result != FMOD_OK)
        {
            return std::unexpected{ OrgErrCode::AudioInitFailed };
        }

        // Getting Version
        mState->coreSystem->getVersion(&mDebugInfo.version);

        // storing Master Channel
        mState->coreSystem->getMasterChannelGroup(&mState->masterGroup);

        return {};
    }



    FMOD_RESULT F_CALL nonblockcallback(FMOD_SOUND* sound, FMOD_RESULT result)
    {
        [[maybe_unused]] auto* snd = reinterpret_cast<FMOD::Sound*>(sound);

        if (result == FMOD_OK)
        {
            char nameBuffer[256];
            snd->getName(nameBuffer, 256);

            LOG(Info, "Async Audio Loaded Successfully: {}", nameBuffer);
        }
        else
        {
            LOG(Error, "Async Audio Load Failed! Code {}: {}", result, FMOD_ErrorString(result));
        }
        return FMOD_OK;
    }

    FMOD_MODE ConvertToFMODMode(AudioStatus status)
    {
        FMOD_MODE mode = FMOD_DEFAULT | FMOD_NONBLOCKING;
        if (status & AudioStatus::IsStreaming)
        {
            mode |= FMOD_CREATESTREAM;
        }
        else
        {
            mode |= FMOD_CREATESAMPLE;
        }


        if (status & AudioStatus::IsLooping)
        {
            mode |= FMOD_LOOP_NORMAL;
        }
        else
        {
            mode |= FMOD_LOOP_OFF;
        }

        return mode;
    }

    static AudioOpenState MapFmodState(FMOD_OPENSTATE state)
    {
        switch (state)
        {
        case FMOD_OPENSTATE_LOADING:    return AudioOpenState::Loading;
        case FMOD_OPENSTATE_READY:      return AudioOpenState::Ready;
        case FMOD_OPENSTATE_ERROR:      return AudioOpenState::Error;
        case FMOD_OPENSTATE_CONNECTING: return AudioOpenState::Connecting;
        default:                        return AudioOpenState::Unknown;
        }
    }


    void System::SetMasterVolume(f32 volume) const
    {
        if (mState->masterGroup)
        {
            mState->masterGroup->setVolume(std::clamp(volume, 0.0f, 1.0f));
        }
    }

    void System::StopAll() const
    {
        if (mState->masterGroup)
        {
            mState->masterGroup->stop();
        }
    }

    void System::UnloadSound(const std::string& name) const
    {
        auto it = mState->soundCache.find(name);
        if (it == mState->soundCache.end()) return;

        FMOD_OPENSTATE state;
        it->second.sound->getOpenState(&state, nullptr, nullptr, nullptr);

        // If it's still loading, we log a warning because this WILL cause a frame hitch
        if (state == FMOD_OPENSTATE_LOADING)
        {
            LOG(Warning, "Hitch Warning: Unloading sound '{}' while still loading!", name);
        }

        it->second.sound->release();
        mState->soundCache.erase(it);
    }

    void System::LoadSound(const std::string& name, const std::string& filepath, AudioStatus mode) const
    {
        if (mState->soundCache.contains(name))
        {
            LOG(Warning, "Sound {} already loaded", name);
        }

        const FMOD_MODE status = ConvertToFMODMode(mode);

        FMOD_CREATESOUNDEXINFO createInfo = {
            .cbsize = sizeof(FMOD_CREATESOUNDEXINFO),
            .nonblockcallback = nonblockcallback,
        };

        SoundAsset newAsset =
        {
            .path = filepath,
            .status = mode,
        };

        FMOD_RESULT result;
        if (mode & AudioStatus::IsStreaming)
        {
            result = mState->coreSystem->createStream(filepath.c_str(), status, &createInfo, &newAsset.sound);
        }
        else
        {
            result = mState->coreSystem->createSound(filepath.c_str(), status, &createInfo, &newAsset.sound);
        }

        if (result != FMOD_OK)
        {
            LOG(Warning, "Failed to create sound! Code {}: {}", result, FMOD_ErrorString(result));
        }

        newAsset.sound->getLength(&newAsset.lengthMs, FMOD_TIMEUNIT_MS);

        mState->soundCache[name] = newAsset;
    }

    void System::PlayAudio(const std::string& name) const
    {
        const auto it = mState->soundCache.find(name);
        if (it == mState->soundCache.end()) return;

        FMOD_OPENSTATE state;
        it->second.sound->getOpenState(&state, nullptr, nullptr, nullptr);
        if (state != FMOD_OPENSTATE_READY) {
            LOG(Debug, "Sound '{}' requested but still loading. Will retry on next update.", name);
            return;
        }

        FMOD_RESULT result = mState->coreSystem->playSound(it->second.sound, nullptr, false, &it->second.channel);

        if (result != FMOD_OK)
        {
            LOG(Error, "Failed to play sound: {}", FMOD_ErrorString(result));
        }
    }

    void System::TogglePause(const std::string& name) const
    {
        auto it = mState->soundCache.find(name);
        if (it == mState->soundCache.end() || !it->second.channel) return;

        bool paused;
        it->second.channel->getPaused(&paused);
        it->second.channel->setPaused(!paused);
    }

    void System::StopSound(const std::string& name) const
    {
        auto it = mState->soundCache.find(name);
        if (it != mState->soundCache.end() && it->second.channel)
        {
            it->second.channel->stop();
        }
    }

    void System::Update() const
    {
        if (mState->coreSystem)
        {
            mState->coreSystem->update();
        }
    }

    SystemDebugInfo& System::GetSystemDebugInfo()
    {
        mDebugInfo.versionString = fmt::format("{}.{:02}.{:02}",
                                               (mDebugInfo.version >> 16), // Major
                                               (mDebugInfo.version >> 8) & 0xFF, // Minor
                                               (mDebugInfo.version & 0xFF) // Patch
        );

        if (mState && mState->coreSystem)
        {
            FMOD_CPU_USAGE usage;
            mState->coreSystem->getCPUUsage(&usage);

            mDebugInfo.cpuDsp      = usage.dsp;
            mDebugInfo.cpuStream   = usage.stream;
            mDebugInfo.cpuGeometry = usage.geometry;
            mDebugInfo.cpuUpdate   = usage.update;

            // Calculate total (including convolution overhead if you use it later)
            mDebugInfo.cpuTotal = usage.dsp + usage.stream + usage.geometry + usage.update +
                                  usage.convolution1 + usage.convolution2;

            // Memory and Channels
            i32 currentAlloc, maxAlloc;
            FMOD_Memory_GetStats(&currentAlloc, &maxAlloc, false);
            mDebugInfo.currentMemoryMB = currentAlloc / Megabyte;
            mDebugInfo.maxMemoryMB = maxAlloc / Megabyte;

            mState->coreSystem->getChannelsPlaying(&mDebugInfo.channelsPlaying, &mDebugInfo.realChannelsPlaying);
        }

        return mDebugInfo;
    }

    Vector<SoundDebugInfo> System::GetDebugInfo() const
    {
        Vector<SoundDebugInfo> infoList;

        if (!mState) return infoList;

        for (const auto& [name, asset] : mState->soundCache)
        {
            SoundDebugInfo info;
            info.name = name;
            info.path = asset.path;
            info.lengthMs = asset.lengthMs;
            info.isStreaming = (asset.status & AudioStatus::IsStreaming);
            info.isLooping = (asset.status & AudioStatus::IsLooping);

            // Essential non-blocking polling
            FMOD_OPENSTATE fState;
            u32 percent;
            bool starving, busy;
            asset.sound->getOpenState(&fState, &percent, &starving, &busy);

            info.state = MapFmodState(fState);

            if (asset.channel)
            {
                bool isPlaying = false;
                asset.channel->isPlaying(&isPlaying);
                if (isPlaying) info.state = AudioOpenState::Ready;
            }

            info.loadProgress = percent;
            info.isStarving = starving;
            info.isDiskBusy = busy;

            infoList.push_back(info);
        }

        return infoList;
    }
}
