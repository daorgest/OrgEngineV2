//
// Created by Orgest on 6/11/2025.
//

#pragma once

#include <algorithm>
#include <bit>
#include <string>
#include <variant>

#include "RenderEnums.h"
#include "Tools/Span.h"
#include "Tools/Vector.h"

namespace Renderer
{
    struct Extent2D
    {
        u32 width = 0;
        u32 height = 0;

        [[nodiscard]] constexpr bool IsZero() const noexcept { return width == 0 || height == 0; }
        bool operator==(const Extent2D&) const = default;
    };

    struct Extent3D
    {
        u32 width = 0;
        u32 height = 0;
        u32 depth = 1;

        bool operator==(const Extent3D&) const = default;
    };

    struct Viewport
    {
        f32 x = 0.f;
        f32 y = 0.f;
        f32 width = 0.f;
        f32 height = 0.f;
        f32 minDepth = 0.f;
        f32 maxDepth = 1.f;
    };


    /// Rasterization & Pipeline State
    struct GpuBlendDesc
    {
        bool enabled = false;
        BlendFactor srcFactor = BlendFactor::SrcAlpha;
        BlendFactor dstFactor = BlendFactor::OneMinusSrcAlpha;
    };

    struct GpuStencilDesc
    {
        bool enabled = false;
        StencilOp passOp = StencilOp::Keep;
        CompareOp compareOp = CompareOp::Always;
        u8 reference = 0;
    };

    struct GpuRasterDesc
    {
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        PolygonMode polygonMode = PolygonMode::Fill;
        CullMode cull = CullMode::Back;

        TextureFormat depthFormat = TextureFormat::UNKNOWN;
        bool depthWrite = true;
        CompareOp depthOp = CompareOp::Less;

        GpuBlendDesc blend;
        GpuStencilDesc stencil;

        SampleCount sampleCount = SampleCount::X2;
        bool alphaToCoverage = false;

        Vector<TextureFormat> colorFormats;

        [[nodiscard]] static GpuRasterDesc Opaque3D(TextureFormat colorFormat, TextureFormat depthFormat)
        {
            GpuRasterDesc desc;
            desc.topology = PrimitiveTopology::TriangleList;
            desc.cull = CullMode::Back;
            desc.depthFormat = depthFormat;
            desc.depthWrite = true;
            desc.depthOp = CompareOp::GreaterOrEqual;
            desc.blend.enabled = false;
            desc.colorFormats.push_back(colorFormat);
            return desc;
        }

        [[nodiscard]] static GpuRasterDesc Transparent(TextureFormat colorFormat, TextureFormat depthFormat)
        {
            GpuRasterDesc desc = Opaque3D(colorFormat, depthFormat);
            desc.depthWrite = false;
            desc.blend.enabled = true;
            desc.blend.srcFactor = BlendFactor::SrcAlpha;
            desc.blend.dstFactor = BlendFactor::OneMinusSrcAlpha;
            return desc;
        }

        [[nodiscard]] static GpuRasterDesc StencilWrite(TextureFormat colorFormat, TextureFormat depthFormat)
        {
            GpuRasterDesc desc = Opaque3D(colorFormat, depthFormat);
            desc.stencil.enabled = true;
            desc.stencil.compareOp = CompareOp::Always;
            desc.stencil.passOp = StencilOp::Replace;
            desc.stencil.reference = 1;
            return desc;
        }

        [[nodiscard]] static GpuRasterDesc StencilRead(TextureFormat colorFormat, TextureFormat depthFormat)
        {
            GpuRasterDesc desc = Opaque3D(colorFormat, depthFormat);
            desc.stencil.enabled = true;
            desc.stencil.compareOp = CompareOp::NotEqual;
            desc.stencil.reference = 1;
            return desc;
        }
    };


    /// Resources: Samplers, Textures, Buffers
    struct SamplerInfo
    {
        SamplerFilter minFilter = SamplerFilter::Linear;
        SamplerFilter magFilter = SamplerFilter::Linear;
        SamplerMipFilter mipFilter = SamplerMipFilter::Linear;

        SamplerAddressMode addressU = SamplerAddressMode::Repeat;
        SamplerAddressMode addressV = SamplerAddressMode::Repeat;
        SamplerAddressMode addressW = SamplerAddressMode::Repeat;

        f32 mipLodBias = 0.0f;
        f32 minLod = FLT_MIN;
        f32 maxLod = FLT_MAX;

        u32 maxAnisotropy = 16;
        bool anisotropyEnable = false;
        bool compareEnable = false;
        bool unnormalizedCoords = false;
        SamplerBorderColor borderColor = SamplerBorderColor::FloatOpaqueBlack;
    };

    constexpr u32 REMAINING_MIP_LEVELS = 0xFFFFFFFF;
    constexpr u32 REMAINING_ARRAY_LAYERS = 0xFFFFFFFF;

    struct GPUTexture;
    struct TextureTransition
    {
        GPUTexture* texture = nullptr;
        TextureLayout oldLayout = TextureLayout::Unknown;
        TextureLayout newLayout = TextureLayout::Unknown;
        u32 baseMipLevel = 0;
        u32 mipLevelCount = REMAINING_MIP_LEVELS;
        u32 baseArrayLayer = 0;
        u32 layerCount = REMAINING_ARRAY_LAYERS;
    };

    struct BufferInfo
    {
        u64 size = 0;
        GPUHeapType heapType = GPUHeapType::Unknown;
        GPUBufferFlag usage = GPUBufferFlag::None;

        [[nodiscard]] static BufferInfo FromPreset(const BufferPreset preset, const u64 size)
        {
            switch (preset)
            {
            case BufferPreset::VertexGPU:
                return {size, GPUHeapType::Default, GPUBufferFlag::Vertex | GPUBufferFlag::ShaderDeviceAddress};
            case BufferPreset::VertexStorageGPU:
                return {
                    size, GPUHeapType::Default,
                    GPUBufferFlag::Vertex | GPUBufferFlag::Storage | GPUBufferFlag::ShaderDeviceAddress
                };
            case BufferPreset::IndexGPU:
                return {size, GPUHeapType::Default, GPUBufferFlag::Index};
            case BufferPreset::UniformHost:
                return {size, GPUHeapType::Upload, GPUBufferFlag::Constant};
            case BufferPreset::StagingUpload:
                return {size, GPUHeapType::Upload, GPUBufferFlag::None};
            case BufferPreset::StagingDownload:
                return {size, GPUHeapType::Readback, GPUBufferFlag::None};
            case BufferPreset::StorageGPU:
                return {size, GPUHeapType::Default, GPUBufferFlag::Storage | GPUBufferFlag::ShaderDeviceAddress};
            case BufferPreset::StorageHostPersistent:
                return {size, GPUHeapType::Upload, GPUBufferFlag::Storage};
            case BufferPreset::SamplerHeapGPU:
                return {size, GPUHeapType::Upload, GPUBufferFlag::DescriptorHeap | GPUBufferFlag::ShaderDeviceAddress};
            case BufferPreset::ResourceHeapGPU:
                return {size, GPUHeapType::Upload, GPUBufferFlag::DescriptorHeap | GPUBufferFlag::ShaderDeviceAddress};
            case BufferPreset::IndirectHost:
                return {size, GPUHeapType::Upload, GPUBufferFlag::Indirect};
            case BufferPreset::IndirectGPU:
                return {
                    size, GPUHeapType::Default,
                    GPUBufferFlag::Indirect | GPUBufferFlag::Storage | GPUBufferFlag::ShaderDeviceAddress
                };
            default:
                return {};
            }
        }
    };

    struct HeapProperties
    {
        u64 samplerDescriptorSize = 0;
        u64 resourceDescriptorSize = 0;
        u64 samplerReservedSize = 0;
        u64 resourceReservedSize = 0;
        u64 samplerHeapAlignment = 0;
        u64 resourceHeapAlignment = 0;
    };

    struct GPUDeviceDesc
    {
        std::string name = "Unknown";
        GPUDeviceType type = GPUDeviceType::Unknown;
        GPUVendor vendor = GPUVendor::UNKNOWN;
        u64 driverVersion = 0;
        u64 dedicatedVideoMemory = 0;
        std::string driverVersionString;
        std::string apiName;
        HeapProperties heapProperties;
    };

    struct TextureInfo
    {
        Extent3D extent = {};
        u16 mipLevels = 1;
        u16 arrayLayers = 1;
        ImageType type = ImageType::Image2D;
        TextureFormat format = TextureFormat::UNKNOWN;
        TextureDimension dimension = TextureDimension::Texture2D;
        mutable ImageUsageFlags usage = ImageUsage::None;
        SampleCount sampleCount = SampleCount::X1;

        void EnableMipmaps()
        {
            if (mipLevels < 1)
            {
                // std::bit_width is a cleaner C++20 alternative to log2 for powers of 2
                const u32 size = std::max(extent.width, extent.height);
                mipLevels = static_cast<u16>(std::bit_width(size));
            }
        }
    };

    struct TextureSwizzleMask
    {
        TextureSwizzle r = TextureSwizzle::Identity;
        TextureSwizzle g = TextureSwizzle::Identity;
        TextureSwizzle b = TextureSwizzle::Identity;
        TextureSwizzle a = TextureSwizzle::Identity;

        bool operator==(const TextureSwizzleMask&) const = default;
    };

    struct TextureViewInfo
    {
        TextureFormat format = TextureFormat::UNKNOWN;
        TextureViewDimension dimension = TextureViewDimension::Auto;
        u32 baseMip = 0;
        u32 mipCount = 1;
        u32 baseLayer = 0;
        u32 arrayCount = 1;
        TextureSwizzleMask swizzle = {};

        bool operator==(const TextureViewInfo&) const = default;
    };

    struct TextureData
    {
        i32 width = 0;
        i32 height = 0;
        i32 depth = 1;
        i32 channels = 4;
        u16 mipLevels = 1;
        u16 arrayLayers = 1;
        TextureFormat format = TextureFormat::UNKNOWN;
        std::variant<Vector<u8>, Vector<f32>> data;
    };

    inline size_t GetMipSize(u32 width, u32 height, u32 blockSize) {
        return std::max(1u, ((width + 3) / 4)) * std::max(1u, (height + 3) / 4) * blockSize;
    }

    /// Bindings & Descriptors
    struct Binding
    {
        u32 binding = 0;
        DescriptorType type = DescriptorType::UniformBuffer;
        ShaderStageFlags stageFlags = ShaderStage::None;
        size_t size = 0;
        u32 count = 1;
        bool isBindless = false;

        bool operator==(const Binding&) const = default;
    };

    struct DescriptorSetLayoutDesc
    {
        u32 setIndex = 0;
        Vector<Binding> bindings;

        [[nodiscard]] static DescriptorSetLayoutDesc FromConstants(const u32 index, Span<const Binding> inBindings)
        {
            DescriptorSetLayoutDesc desc;
            desc.setIndex = index;
            desc.bindings.assign(inBindings);
            return desc;
        }

        bool operator==(const DescriptorSetLayoutDesc&) const = default;
    };

    struct PushConstantDesc
    {
        u32 size = 0;
        u32 offset = 0;
        ShaderStage stages = ShaderStage::None;

        bool operator==(const PushConstantDesc&) const = default;
    };

    struct DescriptorHeapMapping
    {
        u32 setIndex = 0;
        u32 binding = 0;
        u32 bindingCount = 1;
        DescriptorType type = DescriptorType::Unknown;

        MappingSourceType sourceType = MappingSourceType::PushIndex;
        u32 heapBaseIndex = 0;
        u32 pushDataOffset = 0;

        [[nodiscard]] static DescriptorHeapMapping PushImage(u32 set, u32 binding, u32 offset, u32 base = 0)
        {
            return {set, binding, 1, DescriptorType::SampledImage, MappingSourceType::PushIndex, base, offset};
        }

        [[nodiscard]] static DescriptorHeapMapping PushSampler(u32 set, u32 binding, u32 offset, u32 base = 0)
        {
            return {set, binding, 1, DescriptorType::Sampler, MappingSourceType::PushIndex, base, offset};
        }

        [[nodiscard]] static DescriptorHeapMapping PushStorage(u32 set, u32 binding, u32 offset, u32 base = 0)
        {
            return {set, binding, 1, DescriptorType::StorageBuffer, MappingSourceType::PushIndex, base, offset};
        }

        [[nodiscard]] static DescriptorHeapMapping PushUniform(u32 set, u32 binding, u32 offset, u32 base = 0)
        {
            return {set, binding, 1, DescriptorType::UniformBuffer, MappingSourceType::PushIndex, base, offset};
        }

        [[nodiscard]] static DescriptorHeapMapping StaticResource(u32 set, u32 binding, DescriptorType type, u32 index)
        {
            return {set, binding, 1, type, MappingSourceType::ConstantOffset, index, 0};
        }
    };

    template <typename T>
    void SortBindings(Vector<T>& bindings)
    {
        std::ranges::sort(bindings, std::less{}, &T::binding);
    }

    struct PipelineLayoutDesc
    {
        Vector<DescriptorSetLayoutDesc> setLayouts;
        Vector<PushConstantDesc> pushConstants;

        bool operator==(const PipelineLayoutDesc& o) const
        {
            return setLayouts == o.setLayouts && pushConstants == o.pushConstants;
        }

    };

    /// Vertex Input
    struct VertexBindingDescription
    {
        u32 binding = 0;
        u32 stride = 0;
        VertexInputRate inputRate = VertexInputRate::Vertex;
    };

    struct VertexAttributeDescription
    {
        u32 location = 0;
        u32 binding = 0;
        TextureFormat format = TextureFormat::UNKNOWN;
        u32 offset = 0;
    };

    struct VertexInputLayout
    {
        Vector<VertexBindingDescription> bindings;
        Vector<VertexAttributeDescription> attributes;
    };
}
