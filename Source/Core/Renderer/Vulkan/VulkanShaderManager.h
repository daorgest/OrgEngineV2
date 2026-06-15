//
// Created by Orgest on 4/20/2026.
//
#pragma once
#include <mutex>

#include "RenderInterface.h"
#include "VulkanPipeline.h"


namespace Renderer
{
    struct ShaderCompiler;
    struct VulkanDevice;

    struct PendingReload
    {
        std::string path;
        CompileResult result;
    };

    struct VulkanShaderManager final : GPUShaderManager
    {
        void Init(GPUDevice* device, ShaderCompiler* compiler) override;
        ~VulkanShaderManager() override { Destroy(); };
        void Destroy() override;
        void CheckForReloads() override;

        void RegisterPipeline(GPUPipeline* pipeline) override;
        void UnregisterPipeline(GPUPipeline* pipeline) override;

    private:
        static std::filesystem::file_time_type GetNewestShaderTime();

        struct FileWatchData
        {
            std::filesystem::file_time_type lastWriteTime;
            Vector<VulkanPipeline*> dependentPipelines;

            std::atomic<bool> isCompiling = false;
        };

        VulkanDevice* device = nullptr;
        ShaderCompiler* compiler = nullptr;
        std::unordered_map<std::string, FileWatchData> trackedFiles;

        std::chrono::steady_clock::time_point lastCheckTime;
        static constexpr auto checkInterval = std::chrono::milliseconds(500);

        // Async Compilation State
        std::mutex reloadMutex;
        Vector<PendingReload> completedReloads;
        Vector<std::string> failedReloads;
    };

}

