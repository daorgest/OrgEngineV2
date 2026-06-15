//
// Created by Orgest on 5/21/2026.
//

#pragma once
#include "BindlessManager.h"

#include "ShaderConstants.h"

using namespace Renderer;

void BindlessManager::Init(GPUDevice* inDevice, AssetPool<TextureData>& inPool, DescriptorAllocatorGrowable& allocator)
{
    device = inDevice;
    texturePool = &inPool;

    textureLayout2D = DescriptorLayoutBuilder()
                      .AddBindings(Constants::BindlessTextures2D)
                      .Build(device);
    set = allocator.Allocate(textureLayout2D, true, Constants::BindlessTextures2D[0].count);

    textureLayout3D = DescriptorLayoutBuilder()
                      .AddBindings(Constants::BindlessTextures3D)
                      .Build(device);

    set3D = allocator.Allocate(textureLayout3D, true, Constants::BindlessTextures3D[0].count);

    // Create Samplers
    SamplerInfo linRepeat = {
        .minFilter = SamplerFilter::Linear,
        .magFilter = SamplerFilter::Linear,
        .addressU = SamplerAddressMode::Repeat,
        .addressV = SamplerAddressMode::Repeat,
        .maxAnisotropy = 16,
        .anisotropyEnable = true
    };
    linearRepeatSampler = device->CreateSampler(linRepeat);

    SamplerInfo linClamp = linRepeat;
    linClamp.addressU = linClamp.addressV = SamplerAddressMode::ClampToEdge;
    linearClampSampler = device->CreateSampler(linClamp);

    SamplerInfo ptRepeat = {
        .minFilter = SamplerFilter::Nearest,
        .magFilter = SamplerFilter::Nearest
    };
    pointRepeatSampler = device->CreateSampler(ptRepeat);

    SamplerInfo ptClamp = ptRepeat;
    ptClamp.addressU = ptClamp.addressV = SamplerAddressMode::ClampToEdge;
    pointClampSampler = device->CreateSampler(ptClamp);

    // Default Fallbacks
    constexpr u32 whitePixel = 0xFFFFFFFF;
    UploadRawFallback(WhiteIdx, 1, 1, TextureFormat::RGBA8_SRGB, &whitePixel, "Default White", SamplerType::LinearRepeat);

    constexpr u32 flatNormal = 0xFFFF8080; // Packed (0.5, 0.5, 1.0)
    UploadRawFallback(NormalIdx, 1, 1, TextureFormat::RGBA8_UNORM, &flatNormal, "Default Normal", SamplerType::LinearRepeat);

    constexpr u32 magenta = 0xFFFF00FF; // A=255, B=255, G=0, R=255
    constexpr u32 black   = 0xFF000000; // A=255, B=0, G=0, R=0
    constexpr u32 checkerPixels[4] = {
        magenta, black,
        black,   magenta
    };

    // Notice we use PointRepeat here so the checkerboard stays sharp and pixelated
    // instead of blurring into a muddy purple gradient!
    UploadRawFallback(CheckerIdx, 2, 2, TextureFormat::RGBA8_SRGB, checkerPixels, "Default Checkerboard", SamplerType::PointRepeat);
}

void BindlessManager::Destroy()
{
    globalTextures.clear();
    globalTextures3D.clear();
    linearRepeatSampler.reset();
    linearClampSampler.reset();
    pointRepeatSampler.reset();
    pointClampSampler.reset();

    if (device)
    {
        textureLayout2D.Destroy(device);
        textureLayout3D.Destroy(device);
    }
    handleIdToIndex.clear();
    device = nullptr;
}

u32 BindlessManager::Register3DTexture(std::unique_ptr<GPUTexture> texture, SamplerType samplerType)
{
    const u32 newIndex = static_cast<u32>(globalTextures3D.size());

    globalTextures3D.push_back(std::move(texture));

    UpdateDescriptor(set3D, newIndex, globalTextures3D.back()->GetView(), GetSharedSampler(samplerType));
    return newIndex;
}
u32 BindlessManager::GetOrUpload(const ResourceHandle<TextureData> handle, SamplerType samplerType)
{
    if (!handle.IsValid()) return CheckerIdx;

    if (const u32 existingIdx = FindIndex(handle.id); existingIdx != ~0u) return existingIdx;

    const auto res = texturePool->Get(handle);
    if (!res) return CheckerIdx;

    TextureData* cpuData = *res;
    TextureInfo info = {
        .extent = {static_cast<u32>(cpuData->width), static_cast<u32>(cpuData->height), 1},
        .mipLevels = cpuData->mipLevels,
        .arrayLayers = cpuData->arrayLayers,
        .format = cpuData->format,
        .usage = ImageUsage::Sampled | ImageUsage::TransferDst
    };
    info.EnableMipmaps();

    auto gpuTex = device->CreateTexture(info);

    const void* pixelData = std::holds_alternative<Vector<u8>>(cpuData->data)
        ? static_cast<void*>(std::get<Vector<u8>>(cpuData->data).data())
        : static_cast<void*>(std::get<Vector<f32>>(cpuData->data).data());

    gpuTex->UploadData(pixelData);

    const u32 newIndex = static_cast<u32>(globalTextures.size());
    globalTextures.push_back(std::move(gpuTex));
    InsertMapping(handle.id, newIndex);

    UpdateDescriptor(set, newIndex, globalTextures.back()->GetView(), GetSharedSampler(samplerType));

    return newIndex;
}

void BindlessManager::UpdateDescriptor(VulkanDescriptorSet& targetSet, u32 idx, const GPUTextureView* view, const GPUSampler* sampler) const
{
    targetSet.WriteTexture(0, const_cast<GPUTextureView*>(view), const_cast<GPUSampler*>(sampler),
                            DescriptorType::CombinedImageSampler, idx);
    targetSet.Update(device);
}

void BindlessManager::UploadRawFallback(const u32 idx, const u32 w, const u32 h, const TextureFormat fmt, const void* data, const char* name, const SamplerType samplerType)
{
    TextureInfo info = { .extent = {w, h, 1}, .format = fmt, .usage = ImageUsage::Sampled | ImageUsage::TransferDst };
    auto tex = device->CreateTexture(info);
    tex->UploadData(data);
    tex->SetName(name);
    if (globalTextures.size() <= idx) globalTextures.resize(idx + 1);
    globalTextures[idx] = std::move(tex);
    UpdateDescriptor(set, idx, globalTextures[idx]->GetView(), GetSharedSampler(samplerType));
}
