//
// Created by Orgest on 3/7/2026.
//

#pragma once
#include <unordered_map>

#include "DefaultTextures.h"
#include "MeshLoader.h"
#include "../PrimTypes.h"

#include "RenderInterface.h"
#include "VulkanDescriptors.h"

namespace Renderer
{
    struct BindlessManager
    {
        GPUDevice* device = nullptr;
        Assets::AssetRegistry* registry = nullptr;

        // Reserved static indices for engine fallbacks
        static constexpr u32 WhiteIdx   = 0;
        static constexpr u32 NormalIdx  = 1;
        static constexpr u32 CheckerIdx = 2;

        Vector<std::unique_ptr<GPUTexture>> globalTextures;
        std::unordered_map<u32, u32> handleIdToIndex;
        DescriptorSet set;

        std::unique_ptr<GPUSampler> linearSampler;
        std::unique_ptr<GPUSampler> pointSampler;

        void Init(GPUDevice* inDevice, Assets::AssetRegistry& inRegistry, DescriptorAllocatorGrowable& allocator)
        {
            device = inDevice;
            registry = &inRegistry;

            // 1. Setup Descriptor Layout
            const auto layout = DescriptorLayoutBuilder()
                                .AddBindings(Constants::Bindless)
                                .Build(device);

            set = allocator.Allocate(layout, Constants::Bindless[0].count);

            // 2. Initialize Samplers
            SamplerInfo linearInfo = {}; // Defaults to Linear/Repeat
            linearSampler = device->CreateSampler(linearInfo);

            SamplerInfo pointInfo = {
                .minFilter = SamplerFilter::Nearest,
                .magFilter = SamplerFilter::Nearest,
                .mipFilter = SamplerMipFilter::None,
                .addressU  = SamplerAddressMode::ClampToEdge,
                .addressV  = SamplerAddressMode::ClampToEdge
            };
            pointSampler = device->CreateSampler(pointInfo);

            // 3. Force-upload defaults into indices 0, 1, 2
            UploadInternalDefault("internal://white",        GenerateWhite(),   WhiteIdx,   linearSampler.get());
            UploadInternalDefault("internal://normal",       GenerateNormal(),  NormalIdx,  linearSampler.get());
            UploadInternalDefault("internal://checkerboard", GenerateChecker(), CheckerIdx, pointSampler.get());
        }

        u32 GetOrUpload(const TextureHandle handle)
        {
            if (!handle.IsValid()) return WhiteIdx;

            if (auto it = handleIdToIndex.find(handle.id); it != handleIdToIndex.end()) {
                return it->second;
            }

            auto res = registry->texturePool.Get(handle);
            if (!res) return WhiteIdx;
            TextureData* cpuData = *res;

            // Using your exact TextureInfo struct
            TextureInfo info = {
                .extent = { static_cast<u32>(cpuData->width), static_cast<u32>(cpuData->height), 1 },
                .format = cpuData->format,
                .usage  = ImageUsage::Sampled | ImageUsage::TransferDst
            };

            auto gpuTex = device->CreateTexture(info);

            // Safe variant pointer extraction
            const void* pixelData = nullptr;
            if (auto* u8Vec = std::get_if<Vector<u8>>(&cpuData->data)) {
                pixelData = u8Vec->data();
            } else if (auto* f32Vec = std::get_if<Vector<f32>>(&cpuData->data)) {
                pixelData = f32Vec->data();
            }

            gpuTex->UploadData(pixelData);

            u32 newIndex = static_cast<u32>(globalTextures.size());
            globalTextures.push_back(std::move(gpuTex));
            handleIdToIndex[handle.id] = newIndex;

            UpdateDescriptor(newIndex, globalTextures.back().get(), linearSampler.get());
            return newIndex;
        }

        void UpdateMaterialBuffer(const VulkanBuffer* buffer) const
        {
            DescriptorWriter(1, 0, 1)
                .WriteBuffer(Constants::Material[0].binding, buffer, DescriptorType::StorageBuffer)
                .UpdateSet(device, set);
        }

    private:
        void UpdateDescriptor(const u32 idx, const GPUTexture* tex, const GPUSampler* sampler) const
        {
            DescriptorWriter(1, 1, 0)
                .WriteCombinedImage(Constants::Bindless[0].binding, tex, sampler, idx)
                .UpdateSet(device, set);
        }

        void UploadInternalDefault(const std::string& name, TextureData&& data, const u32 targetIdx, const GPUSampler* sampler)
        {

            auto handle = registry->texturePool.Load(
                name, [d = std::move(data)](const std::string&) mutable -> Result<TextureData>
                {
                    return std::move(d);
                }).value();

            TextureData* cpuData = *registry->texturePool.Get(handle);
            TextureInfo info = {
                .extent = { static_cast<u32>(cpuData->width), static_cast<u32>(cpuData->height), 1 },
                .format = cpuData->format,
                .usage  = ImageUsage::Sampled | ImageUsage::TransferDst
            };
            auto gpuTex = device->CreateTexture(info);

            gpuTex->UploadData(std::get<Vector<u8>>(cpuData->data).data());

            globalTextures.push_back(std::move(gpuTex));
            handleIdToIndex[handle.id] = targetIdx;
            UpdateDescriptor(targetIdx, globalTextures.back().get(), sampler);
        }

        // Generators
        static TextureData GenerateWhite() {
            u32 pixel = 0xFFFFFFFF;
            return TextureLoader::BuildProceduralData((u8*)&pixel, 1, 1, true).value();
        }
        static TextureData GenerateNormal() {
            const u32 flatNormal = PackUnorm4x8({0.5f, 0.5f, 1.0f, 1.0f});
            return TextureLoader::BuildProceduralData((u8*)&flatNormal, 1, 1, false).value();
        }
        static TextureData GenerateChecker() {
            constexpr u32 size = 16;
            Array<u32, size * size> pixels;
            for (u32 y = 0; y < size; ++y) {
                for (u32 x = 0; x < size; ++x) {
                    pixels[y * size + x] = ((x / 4 + y / 4) % 2 == 0) ? 0xFFFFFFFF : 0xFF333333;
                }
            }
            return TextureLoader::BuildProceduralData((u8*)pixels.data(), size, size, true).value();
        }
    };
}
