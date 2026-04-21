//
// Created by Orgest on 3/7/2026.
//

#pragma once
#include <unordered_map>

#include "../PrimTypes.h"

#include "RenderInterface.h"
#include "ShaderConstants.h"
#include "VulkanDescriptors.h"
#include "Tools/AssetPool.h"

#include "MathFuncs.h"
#include "Tools/Array.h"

namespace Renderer
{
    struct BindlessManager
    {
        GPUDevice* device = nullptr;
        AssetPool<TextureData>* texturePool = nullptr;

        // Reserved static indices for engine fallbacks
        static constexpr u32 WhiteIdx = 0;
        static constexpr u32 NormalIdx = 1;
        static constexpr u32 CheckerIdx = 2;
        static constexpr u32 BlackIdx = 3;

        Vector<std::unique_ptr<GPUTexture>> globalTextures;
        std::unordered_map<u64, u32> handleIdToIndex;
        DescriptorSet set;

        std::unique_ptr<GPUSampler> linearSampler;
        std::unique_ptr<GPUSampler> pointSampler;

        void Init(GPUDevice* inDevice, AssetPool<TextureData>& inPool, DescriptorAllocatorGrowable& allocator)
        {
            device = inDevice;
            texturePool = &inPool;


            const auto layout = DescriptorLayoutBuilder()
                                .AddBindings(Constants::BindlessTextures)
                                .Build(device);

            set = allocator.Allocate(layout, true, Constants::BindlessTextures[0].count);


            SamplerInfo linearInfo = {
                .minLod = 0.0f,
                .maxLod = 12.0f,
                .maxAnisotropy = 16,
                .anisotropyEnable = true
            };
            linearSampler = device->CreateSampler(linearInfo);

            SamplerInfo pointInfo = {
                .minFilter = SamplerFilter::Nearest,
                .magFilter = SamplerFilter::Nearest,
                .mipFilter = SamplerMipFilter::None,
                .addressU = SamplerAddressMode::ClampToEdge,
                .addressV = SamplerAddressMode::ClampToEdge
            };
            pointSampler = device->CreateSampler(pointInfo);

            // Slot 0: White
            constexpr u32 whitePixel = 0xFFFFFFFF;
            UploadRawFallback(WhiteIdx, 1, 1, TextureFormat::RGBA8_SRGB, &whitePixel, "Default White");

            // Slot 1: Normal (128, 128, 255)
            const u32 flatNormal = PackUnorm4x8({0.5f, 0.5f, 1.0f, 1.0f});
            UploadRawFallback(NormalIdx, 1, 1, TextureFormat::RGBA8_UNORM, &flatNormal, "Default Normal");

            // Slot 2: Checkerboard
            constexpr u32 cSize = 16;
            Array<u32, cSize * cSize> pixels;
            for (u32 y = 0; y < cSize; ++y)
            {
                for (u32 x = 0; x < cSize; ++x)
                {
                    pixels[y * cSize + x] = ((x / 4 + y / 4) % 2 == 0) ? 0xFFFF00FF : 0xFF000000;
                }
            }
            UploadRawFallback(CheckerIdx, cSize, cSize, TextureFormat::RGBA8_SRGB, pixels.data(), "Default Checker");
        }

        u32 GetOrUpload(const ResourceHandle<TextureData> handle)
        {
            if (!handle.IsValid()) return CheckerIdx;

            if (auto it = handleIdToIndex.find(handle.id); it != handleIdToIndex.end())
            {
                return it->second;
            }

            const auto res = texturePool->Get(handle);
            if (!res) return CheckerIdx;
            TextureData* cpuData = *res;


            TextureInfo info = {
                .extent = {static_cast<u32>(cpuData->width), static_cast<u32>(cpuData->height), 1},
                .format = cpuData->format,
                .usage = ImageUsage::Sampled | ImageUsage::TransferDst
            };

            info.EnableMipmaps();

            auto gpuTex = device->CreateTexture(info);

            // Safe variant pointer extraction
            const void* pixelData = nullptr;
            if (auto* u8Vec = std::get_if<Vector<u8>>(&cpuData->data))
            {
                pixelData = u8Vec->data();
            }
            else if (auto* f32Vec = std::get_if<Vector<f32>>(&cpuData->data))
            {
                pixelData = f32Vec->data();
            }

            gpuTex->UploadData(pixelData);

            const u32 newIndex = static_cast<u32>(globalTextures.size());
            globalTextures.push_back(std::move(gpuTex));
            handleIdToIndex[handle.id] = newIndex;

            UpdateDescriptor(newIndex, globalTextures.back().get(), linearSampler.get());
            return newIndex;
        }

    private:
        void UpdateDescriptor(u32 idx, const GPUTexture* tex, const GPUSampler* sampler) const
        {
            DescriptorWriter writer;
            // Write to Set 2, Binding 0
            writer.WriteCombinedImage(0, tex, sampler, idx);
            writer.UpdateSet(device, set);
        }

        void UploadRawFallback(u32 idx, u32 w, u32 h, TextureFormat fmt, const void* data, const char* name)
        {
            TextureInfo info = {
                .extent = {w, h, 1},
                .format = fmt,
                .usage = ImageUsage::Sampled | ImageUsage::TransferDst
            };

            auto tex = device->CreateTexture(info);
            tex->UploadData(data);
            tex->SetName(name);

            if (globalTextures.size() <= idx) globalTextures.resize(idx + 1);
            globalTextures[idx] = std::move(tex);
            UpdateDescriptor(idx, globalTextures[idx].get(), linearSampler.get());
        }
    };
}
