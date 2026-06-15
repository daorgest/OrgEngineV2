//
// Created by Orgest on 3/7/2026.
//

#pragma once
#include "RendererTypes.h"
#include "VulkanDescriptors.h"
#include "Tools/AssetPool.h"

namespace Renderer
{
    struct GPUTexture;
    struct GPUDevice;

    // Predefined shared filtering setups
    enum class SamplerType : u8
    {
        LinearRepeat, // Standard tiling meshes (Albedo, Normals)
        LinearClamp, // Post-processing, UI, LUTs
        PointRepeat, // Pixel art, discrete data masks
        PointClamp // Shadow maps, depth buffers, G-Buffer lookups
    };

    struct TextureMapping
    {
        u64 id;
        u32 index;

        bool operator<(const TextureMapping& other) const { return id < other.id; }
    };

    struct BindlessManager
    {
        GPUDevice* device = nullptr;
        AssetPool<TextureData>* texturePool = nullptr;

        static constexpr u32 WhiteIdx = 0;
        static constexpr u32 NormalIdx = 1;
        static constexpr u32 CheckerIdx = 2;
        static constexpr u32 BlackIdx = 3;

        Vector<std::unique_ptr<GPUTexture>> globalTextures;
        Vector<TextureMapping> handleIdToIndex;

        VulkanDescriptorSet set;
        DescriptorLayout textureLayout2D;

        Vector<std::unique_ptr<GPUTexture>> globalTextures3D;
        VulkanDescriptorSet set3D;
        DescriptorLayout textureLayout3D;

        std::unique_ptr<GPUSampler> linearRepeatSampler;
        std::unique_ptr<GPUSampler> linearClampSampler;
        std::unique_ptr<GPUSampler> pointRepeatSampler;
        std::unique_ptr<GPUSampler> pointClampSampler;

        [[nodiscard]] GPUSampler* GetSharedSampler(SamplerType type) const noexcept
        {
            switch (type)
            {
            case SamplerType::LinearRepeat: return linearRepeatSampler.get();
            case SamplerType::LinearClamp: return linearClampSampler.get();
            case SamplerType::PointRepeat: return pointRepeatSampler.get();
            case SamplerType::PointClamp: return pointClampSampler.get();
            default: return linearRepeatSampler.get();
            }
        }

        void Init(GPUDevice* inDevice, AssetPool<TextureData>& inPool, DescriptorAllocatorGrowable& allocator);
        void Destroy();

        u32 Register3DTexture(std::unique_ptr<GPUTexture> tex, SamplerType samplerType = SamplerType::PointClamp);

        // Cache-friendly lookup via binary search
        u32 GetOrUpload(ResourceHandle<TextureData> handle, SamplerType samplerType = SamplerType::LinearRepeat);

        void UpdateDescriptor(VulkanDescriptorSet& targetSet, u32 idx, const GPUTextureView* view,
                              const GPUSampler* sampler) const;

    private:
        [[nodiscard]] u32 FindIndex(u64 id) const
        {
            if (const auto it = std::lower_bound(handleIdToIndex.begin(), handleIdToIndex.end(), TextureMapping{id, 0});
                it != handleIdToIndex.end() && it->id == id) return it->index;
            return ~0u;
        }

        void InsertMapping(u64 id, u32 index)
        {
            const auto it = std::lower_bound(handleIdToIndex.begin(), handleIdToIndex.end(), TextureMapping{id, index});


            const vecSizeType insertPos = static_cast<vecSizeType>(it - handleIdToIndex.begin());

            handleIdToIndex.insert(insertPos, {id, index});
        }

        void UploadRawFallback(u32 idx, u32 w, u32 h, TextureFormat fmt, const void* data, const char* name,
                               SamplerType samplerType);
    };
}
