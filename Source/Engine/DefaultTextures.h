//
// Created by Orgest on 12/29/2025.
//

#pragma once
#include <memory>

#include "RendererTypes.h"
#include "RenderInterface.h"
#include "Tools/Array.h"
#include "../PrimTypes.h"
#include "MathFuncs.h"

namespace Renderer
{
    struct GPUDevice;
    struct GPUTexture;

    struct TextureDefaults
    {
        std::unique_ptr<GPUTexture> white;
        std::unique_ptr<GPUTexture> normal;
        std::unique_ptr<GPUTexture> checkerboard;

        std::unique_ptr<GPUSampler> linearSampler;
        std::unique_ptr<GPUSampler> pointSampler;

        void Init(GPUDevice* device)
        {
            white        = CreateWhite(device);
            normal       = CreateNormal(device);
            checkerboard = CreateCheckerboard(device);

            SamplerInfo linearInfo = {}; // by default its linear/repeat
            linearSampler = device->CreateSampler(linearInfo);

            SamplerInfo pointInfo = { // for checkerboard
                .minFilter = SamplerFilter::Nearest,
                .magFilter = SamplerFilter::Nearest,
                .mipFilter = SamplerMipFilter::None,
                .addressU  = SamplerAddressMode::ClampToEdge,
                .addressV  = SamplerAddressMode::ClampToEdge
            };
            pointSampler = device->CreateSampler(pointInfo);
        }
    private:

        static std::unique_ptr<GPUTexture> CreateBaseTexture(GPUDevice* device, u32 width, u32 height, TextureFormat format,
            const void* data, const char* debugName)
        {
            TextureInfo info = {
                .extent     = { width, height, 1 },
                .mipLevels  = 1,
                .type       = ImageType::Image2D,
                .format     = format,
                .dimension  = TextureDimension::Texture2D,
                .usage      = ImageUsage::Sampled | ImageUsage::TransferDst
            };

            auto tex = device->CreateTexture(info);
            tex->UploadData(data);
            tex->SetName(debugName);
            return tex;
        }

        static std::unique_ptr<GPUTexture> CreateWhite(GPUDevice* device)
        {
            // 0xFFFFFFFF is full opaque white (RGBA8)
            constexpr u32 whitePixel = 0xFFFFFFFF;
            return CreateBaseTexture(device, 1, 1, TextureFormat::RGBA8_SRGB, &whitePixel, "Default White");
        }

        static std::unique_ptr<GPUTexture> CreateNormal(GPUDevice* device)
        {
            // Flat Tangent Space Normal: (0.5, 0.5, 1.0) -> [128, 128, 255, 255]
            const u32 flatNormal = PackUnorm4x8({0.5f, 0.5f, 1.0f, 1.0f});
            return CreateBaseTexture(device, 1, 1, TextureFormat::RGBA8_UNORM, &flatNormal, "Default Normal");
        }

        static std::unique_ptr<GPUTexture> CreateCheckerboard(GPUDevice* device)
        {
            constexpr u32 size = 16;
            Array<u32, size * size> pixels;

            for (u32 y = 0; y < size; ++y) {
                for (u32 x = 0; x < size; ++x)
                {
                    constexpr u32 colorB = 0xFF333333;
                    constexpr u32 colorA = 0xFFFFFFFF;
                    // Standard checker pattern logic
                    pixels[y * size + x] = ((x / 4 + y / 4) % 2 == 0) ? colorA : colorB;
                }
            }

            return CreateBaseTexture(device, size, size, TextureFormat::RGBA8_SRGB, pixels.data(), "Default Checkerboard");
        }
    };
}
