//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <algorithm>

#include "Audio.h"
#include <fmod_studio.hpp>
#include <fmod_errors.h>
#include <ranges>
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
    struct AudioState
    {
        FMOD::Studio::System* studioSystem = nullptr;
        FMOD::System* coreSystem = nullptr;
        std::unordered_map<std::string, FMOD::Studio::Bank*> bankCache;
    };

    System::System() : mState(std::make_unique<AudioState>()) {}

    System::~System()
    {
        if (mState->studioSystem)
        {
            mState->studioSystem->release();
        }
    }

    Result<void> System::Init()
    {
        FMOD_RESULT result = FMOD::Studio::System::create(&mState->studioSystem);
        if (result != FMOD_OK) return std::unexpected{ OrgErrCode::AudioInitFailed };

        result = mState->studioSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr);
        if (result != FMOD_OK) return std::unexpected{ OrgErrCode::AudioInitFailed };


        mState->studioSystem->getCoreSystem(&mState->coreSystem);
        if (mState->coreSystem)
        {
            mState->coreSystem->getVersion(&mDebugInfo.version);
        }

        return {};
    }

    void System::LoadBank(const std::string& filepath) const
    {
        if (mState->bankCache.contains(filepath)) return;

        FMOD::Studio::Bank* bank = nullptr;
        FMOD_RESULT result = mState->studioSystem->loadBankFile(filepath.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);

        if (result == FMOD_OK)
        {
            mState->bankCache[filepath] = bank;
            LOG(Info, "Successfully loaded Studio Bank: {}", filepath);
        }
        else
        {
            LOG(Error, "Failed to load Bank {}: {}", filepath, FMOD_ErrorString(result));
        }
    }

    void System::UnloadAllBanks() const
    {
        for (const auto& bank : mState->bankCache | std::views::values)
        {
            if (bank)
            {
                bank->unload();
            }
        }
        mState->bankCache.clear();
    }

    void System::PlayOneShot(const std::string& eventPath) const
    {
        FMOD::Studio::EventDescription* eventDesc = nullptr;

        if (mState->studioSystem->getEvent(eventPath.c_str(), &eventDesc) != FMOD_OK)
        {
            LOG(Warning, "Event path not found: {}", eventPath);
            return;
        }

        FMOD::Studio::EventInstance* instance = nullptr;
        eventDesc->createInstance(&instance);

        if (instance)
        {
            instance->start();
            instance->release();
        }
    }

    void System::SetMasterVolume(f32 volume) const
    {
        FMOD::Studio::Bus* masterBus = nullptr;
        if (mState->studioSystem->getBus("bus:/", &masterBus) == FMOD_OK)
        {
            masterBus->setVolume(std::clamp(volume, 0.0f, 1.0f));
        }
    }

    void System::StopAll() const
    {
        FMOD::Studio::Bus* masterBus = nullptr;
        if (mState->studioSystem->getBus("bus:/", &masterBus) == FMOD_OK)
        {
            masterBus->stopAllEvents(FMOD_STUDIO_STOP_ALLOWFADEOUT);
        }
    }

    void System::Update() const
    {
        if (mState->studioSystem)
        {
            mState->studioSystem->update();
        }
    }

    SystemDebugInfo& System::GetSystemDebugInfo()
    {
        mDebugInfo.versionString = fmt::format("{}.{:02}.{:02}",
                                               (mDebugInfo.version >> 16),
                                               (mDebugInfo.version >> 8) & 0xFF,
                                               (mDebugInfo.version & 0xFF));

        if (mState && mState->studioSystem)
        {
            FMOD_STUDIO_CPU_USAGE studioUsage;
            FMOD_CPU_USAGE coreUsage;
            mState->studioSystem->getCPUUsage(&studioUsage, &coreUsage);

            mDebugInfo.cpuStudioUpdate = studioUsage.update;
            mDebugInfo.cpuCoreDsp = coreUsage.dsp;

            i32 currentAlloc, maxAlloc;
            FMOD_Memory_GetStats(&currentAlloc, &maxAlloc, false);
            mDebugInfo.currentMemoryMB = static_cast<float>(currentAlloc) / 1_MiB;

            if (mState->coreSystem)
            {
                mState->coreSystem->getChannelsPlaying(&mDebugInfo.channelsPlaying, nullptr);
            }
        }

        return mDebugInfo;
    }
}
