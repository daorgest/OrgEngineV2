//
// Created by Orgest on 4/20/2026.
//

#include "VulkanShaderManager.h"
#include "ShaderCompiler.h"
#include <ranges>

#include "VulkanDevice.h"

using namespace Renderer;

void VulkanShaderManager::Init(GPUDevice* inDevice, ShaderCompiler* inCompiler)
{
    device = static_cast<VulkanDevice*>(inDevice);
    compiler = inCompiler;
    lastCheckTime = std::chrono::steady_clock::now();
}

void VulkanShaderManager::Destroy()
{
    std::lock_guard lock(reloadMutex);
    trackedFiles.clear();
}

void VulkanShaderManager::RegisterPipeline(GPUPipeline* pipeline)
{
    auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);

    // Extract the exact path directly from the pipeline!
    const std::string sourcePath = vkPipeline->GetSourcePath().data();
    if (sourcePath.empty()) return;

    const std::string fullPath = std::string(ENGINE_SOURCE_DIR) + "/" + sourcePath;
    auto& watchData = trackedFiles[fullPath];

    if (watchData.dependentPipelines.empty())
    {
        std::error_code ec;
        watchData.lastWriteTime = std::filesystem::last_write_time(fullPath, ec);
    }

    watchData.dependentPipelines.push_back(vkPipeline);
}

std::filesystem::file_time_type VulkanShaderManager::GetNewestShaderTime()
{
    std::filesystem::file_time_type newestTime = {};
    std::error_code ec;

    // Search both of your shader directories
    const std::string dirs[] = {
        std::string(ENGINE_SOURCE_DIR) + "/Shaders",
        std::string(ENGINE_SOURCE_DIR) + "/Source/Game/Shaders"
    };

    for (const auto& dir : dirs)
    {
        if (!std::filesystem::exists(dir, ec)) continue;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".slang")
            {
                auto time = std::filesystem::last_write_time(entry.path(), ec);
                if (time > newestTime) newestTime = time;
            }
        }
    }
    return newestTime;
}

void VulkanShaderManager::UnregisterPipeline(GPUPipeline* pipeline)
{
    auto* vkPipeline = static_cast<VulkanPipeline*>(pipeline);
    for (auto& watchData : trackedFiles | std::views::values)
    {
        auto& vec = watchData.dependentPipelines;
        const auto it = std::ranges::find(vec, vkPipeline);

        if (it != vec.end())
        {
            // Swap with the last element and pop
            *it = vec.back();
            vec.pop_back();
        }
    }
}

void VulkanShaderManager::CheckForReloads()
{
    // Process any async compilations that finished on the background threads
    {
        std::lock_guard lock(reloadMutex);

        // Process successful compiles
        for (const auto& reload : completedReloads)
        {
            if (auto it = trackedFiles.find(reload.path); it != trackedFiles.end())
            {
                for (VulkanPipeline* pipe : it->second.dependentPipelines)
                {
                    pipe->ApplyReload(reload.result);
                }
                // Safely unlock the file for future edits
                it->second.isCompiling = false;
            }
        }

        // Reset compiling flags for failed compiles so they can be attempted again
        for (const auto& failedPath : failedReloads)
        {
            auto it = trackedFiles.find(failedPath);
            if (it != trackedFiles.end())
            {
                it->second.isCompiling = false;
            }
        }

        completedReloads.clear();
        failedReloads.clear();
    }

    // Throttle the filesystem checks (every 500ms)
    const auto now = std::chrono::steady_clock::now();
    if (now - lastCheckTime < checkInterval) return;
    lastCheckTime = now;

    // Find the absolute newest shader file modification in the project
    const auto globalNewestTime = GetNewestShaderTime();

    for (auto& [path, watchData] : trackedFiles)
    {
        if (watchData.dependentPipelines.empty()) continue;

        // Skip if this file is already compiling to prevent thread spam
        if (watchData.isCompiling) continue;

        std::error_code ec;
        auto currentWriteTime = std::filesystem::last_write_time(path, ec);

        // If the main file changed, OR a shared global file changed
        if ((!ec && currentWriteTime > watchData.lastWriteTime) || globalNewestTime > watchData.lastWriteTime)
        {
            LOG(Info, "[ShaderManager] Queuing compilation: {}", path);

            watchData.lastWriteTime = std::max(currentWriteTime, globalNewestTime);
            watchData.isCompiling = true;

            // Fire and forget async task
            std::jthread([this, path]
            {
                CompileResult res = compiler->CompileShader(path);

                std::lock_guard lock(reloadMutex);
                if (!res.code.empty())
                {
                    completedReloads.push_back({path, std::move(res)});
                }
                else
                {
                    LOG(Warning, "[ShaderManager] Hot-Reload compilation failed for '{}'. Rejecting changes and falling back..", path);
                                    failedReloads.push_back(path);
                }
            }).detach();
        }
    }
}
