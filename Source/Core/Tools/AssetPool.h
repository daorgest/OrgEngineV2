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
    std::string path;
    u32 refCount = 0;
    u32 generation = 0;
};

template <typename T>
class AssetPool
{
    Vector<ResourceSlot<T>> resources;
    Vector<u32> freeList;
    std::unordered_map<std::string, u32> registry;

public:
    explicit AssetPool(u32 initialCapacity = 2048)
    {
        resources.reserve(initialCapacity);
        LOG(Info, "ResourceManager: Initialized with {} slots.", initialCapacity);
    }

    template <typename Loader>
    [[nodiscard]] Result<ResourceHandle<T>> Load(const std::filesystem::path& path, Loader&& loader)
    {
        const std::string pathStr = path.string();

        if (auto it = registry.find(pathStr); it != registry.end())
        {
            u32 idx = it->second;
            ++resources[idx].refCount;
            return ResourceHandle<T>(idx, resources[idx].generation);
        }

        u32 idx = PrepareNextIndex();
        auto& slot = resources[idx];


        auto result = loader(pathStr);
        if (!result)
        {
            CleanupFailedIndex(idx);
            return std::unexpected(result.error());
        }


        slot.data = std::move(*result);
        slot.path = pathStr;
        slot.refCount = 1;
        registry[pathStr] = idx;

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
            return Result<PtrType>(std::unexpected(OrgErrCode::StaleHandle));
        }

        return Result<PtrType>(&slot.data);
    }

    void Release(ResourceHandle<T> handle)
    {
        if (!handle.IsValid()) return;

        u32 idx = handle.index();
        if (idx >= resources.size()) return;

        auto& slot = resources[idx];
        if (slot.generation != handle.gen()) return;

        if (--slot.refCount == 0)
        {
            // O(1) Removal: We stored the path in the slot specifically for this
            registry.erase(slot.path);

            // Clean up
            slot.data = T{};
            slot.path.clear();

            // Increment generation to invalidate all existing handles to this slot
            ++slot.generation;
            freeList.push_back(idx);
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
