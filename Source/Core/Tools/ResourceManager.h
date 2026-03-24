#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include "Logger.h"
#include "Vector.h"
#include "../PrimTypes.h"


template <typename T>
struct ResourceSlot
{
    T data{};
#if defined(_DEBUG) || defined(ENGINE_DEBUG)
    std::string name;
#endif
    u32 refCount = 0;
    u32 generation = 0;
};

template <typename T>
class ResourceManager
{
    Vector<ResourceSlot<T>> resources;
    Vector<u32> freeList;
    std::unordered_map<std::string, u32> registry;

public:
    explicit ResourceManager(u32 initialCapacity = 2048)
    {
        resources.reserve(initialCapacity);
        LOG(Info, "ResourceManager: Initialized with {} slots.", initialCapacity);
    }

    template <typename Loader>
    [[nodiscard]] Result<ResourceHandle<T>> Load(const std::filesystem::path& path, Loader&& loader)
    {
        const std::string pathStr = path.string();

        if (const auto it = registry.find(pathStr); it != registry.end())
        {
            u32 idx = it->second;
            ++resources[idx].refCount;
            LOG(Debug, "ResourceManager: Resource '{}' already loaded at index {}. RefCount: {}.",
                pathStr, idx, resources[idx].refCount);
            return ResourceHandle<T>(idx, resources[idx].generation);
        }

        u32 idx = PrepareNextIndex();

        auto result = loader(pathStr);
        if (!result)
        {
            CleanupFailedIndex(idx);
            LOG(Error, "ResourceManager: Failed to load resource from path: '{}'.", pathStr);
            return std::unexpected(result.error());
        }

        auto& slot = resources[idx];
#if defined(_DEBUG) || defined(ENGINE_DEBUG)
        slot.name = pathStr;
#endif
        slot.refCount = 1;
        slot.data = std::move(*result);
        registry[pathStr] = idx;

        LOG(Info, "ResourceManager: Loaded '{}' into slot {} (Gen: {}).", pathStr, idx, slot.generation);
        return ResourceHandle<T>(idx, slot.generation);
    }

    [[nodiscard]] auto Get(this auto&& self, ResourceHandle<T> handle) noexcept
    {
        using PtrType = decltype(&self.resources[0].data);
        if (!handle.IsValid()) return Result<PtrType>(std::unexpected(OrgErrCode::AssetNotFound));

        u32 idx = handle.index();
        if (idx >= self.resources.size()) return Result<PtrType>(std::unexpected(OrgErrCode::AssetNotFound));

        auto& slot = self.resources[idx];
        if (slot.generation != handle.gen())
        {
            LOG(Error, "ResourceManager: Accessing stale handle at index {}.", idx);
            return Result<PtrType>(std::unexpected(OrgErrCode::StaleHandle));
        }

        return Result<PtrType>(&slot.data);
    }

    void Release(ResourceHandle<T> handle)
    {
        if (!handle.IsValid()) return;

        u32 index = handle.index();
        if (index >= resources.size()) return;

        auto& slot = resources[index];
        if (slot.generation != handle.gen()) return;

        if (--slot.refCount == 0)
        {
            // Registry lookup still needs a key even if slot.name is ifdef'd out
            // We find it by searching the registry for the index
            for (auto it = registry.begin(); it != registry.end(); ++it)
            {
                if (it->second == index)
                {
                    registry.erase(it);
                    break;
                }
            }

            slot.data = T{};
            ++slot.generation;
            freeList.push_back(index);
            LOG(Info, "ResourceManager: Releasing resource at index {}. Returning to freeList.", index);
        }
    }

private:
    u32 PrepareNextIndex()
    {
        if (!freeList.empty())
        {
            u32 idx = freeList.back();
            freeList.pop_back();
            return idx;
        }

        if (resources.size() >= resources.capacity())
        {
            LOG(Warning, "!!! ResourceManager: CRITICAL CAPACITY REACHED ({}) !!!", resources.capacity());
        }

        resources.emplace_back();
        return static_cast<u32>(resources.size() - 1);
    }

    void CleanupFailedIndex(u32 idx)
    {
        if (idx == resources.size() - 1) resources.pop_back();
        else freeList.push_back(idx);
    }
};
