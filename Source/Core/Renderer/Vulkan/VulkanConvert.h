//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include <cassert>
#include <span>
#include <volk.h>

#include "RendererTypes.h"

namespace Renderer
{
    // Shader Stage conversions
    [[nodiscard]] constexpr VkShaderStageFlags ToVk(ShaderStageFlags flags)
    {
        return std::to_underlying(flags);
    }

    // LoadOp conversions
    [[nodiscard]] constexpr VkAttachmentLoadOp ToVk(LoadOP op)
    {
        switch (op)
        {
        case LoadOP::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOP::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOP::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    // StoreOp conversions
    [[nodiscard]] constexpr VkAttachmentStoreOp ToVk(StoreOp op)
    {
        switch (op)
        {
        case StoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    [[nodiscard]] constexpr VkPrimitiveTopology ToVk(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::TriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    [[nodiscard]] constexpr VkCullModeFlags ToVk(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None: return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
        case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
        default: return VK_CULL_MODE_BACK_BIT;
        }
    }

    [[nodiscard]] constexpr VkCompareOp ToVk(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Never: return VK_COMPARE_OP_NEVER;
        case CompareOp::Less: return VK_COMPARE_OP_LESS;
        case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
        default: return VK_COMPARE_OP_LESS_OR_EQUAL;
        }
    }

    [[nodiscard]] constexpr VkStencilOp ToVk(StencilOp op)
    {
        switch (op)
        {
        case StencilOp::Keep: return VK_STENCIL_OP_KEEP;
        case StencilOp::Zero: return VK_STENCIL_OP_ZERO;
        case StencilOp::Replace: return VK_STENCIL_OP_REPLACE;
        case StencilOp::Increment: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOp::Invert: return VK_STENCIL_OP_INVERT;
        default: return VK_STENCIL_OP_KEEP;
        }
    }

    [[nodiscard]] constexpr VkBlendFactor ToVk(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        default: return VK_BLEND_FACTOR_ONE;
        }
    }


    [[nodiscard]] constexpr auto ToPreset(DescriptorType type) -> BufferPreset
    {
        switch (type)
        {
        case DescriptorType::UniformBuffer: return BufferPreset::UniformHost;
        case DescriptorType::StorageBuffer: return BufferPreset::StorageHostPersistent;
        default: assert(false && "Unsupported descriptor type");
            return BufferPreset::UniformHost;
        }
    }

    /**
     * @brief Selects the best available VkPresentModeKHR based on hardware support.
     * We pass the available modes from the swapchain because only FIFO is guaranteed;
     * requesting an unsupported mode like Mailbox or Immediate will cause a device crash.
     */
    [[nodiscard]] constexpr VkPresentModeKHR ToVkPresentMode(PresentMode mode, std::span<const VkPresentModeKHR> available)
    {
        auto has = [&](VkPresentModeKHR m)
        {
            return std::ranges::find(available, m) != available.end();
        };

        switch (mode)
        {
        case PresentMode::LowLatency:
            if (has(VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
            if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
            return VK_PRESENT_MODE_FIFO_KHR;

        case PresentMode::VSyncOff:
            if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
            if (has(VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
            return VK_PRESENT_MODE_FIFO_KHR;

        case PresentMode::Adaptive:
            if (has(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            return VK_PRESENT_MODE_FIFO_KHR;

        case PresentMode::VSyncOn:
        default:
            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }

    struct SyncState
    {
        VkPipelineStageFlags2 stageMask;
        VkAccessFlags2 accessMask;
    };

    // Wanted to not go insane pasting this everywhere...layout transitions ew
    [[nodiscard]] constexpr SyncState GetSyncState(const TextureLayout layout, const bool isDestination)
    {
        switch (layout)
        {
        case TextureLayout::Unknown:
            return {VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};

        case TextureLayout::ShaderReadOnly:
            return {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};

        case TextureLayout::ColorWrite:
            return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};

        case TextureLayout::DepthWrite:
            // Source waits for late tests; destination starts at early tests
            return {
                isDestination
                    ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                    : VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
            };

        case TextureLayout::CopyDestination:
            return {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};

        case TextureLayout::CopySource:
            return {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};

        case TextureLayout::Present:
            return {VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};

        case TextureLayout::General:
        default:
            return {
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
                VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT // this combo is for nsight
            };
        }
    }

    // Images
    [[nodiscard]] constexpr VkImageType ToVkImageType(TextureDimension dimension)
    {
        switch (dimension)
        {
        case TextureDimension::Texture1D:
            return VK_IMAGE_TYPE_1D;
        case TextureDimension::Texture2D:
            return VK_IMAGE_TYPE_2D;
        case TextureDimension::Texture3D:
            return VK_IMAGE_TYPE_3D;
        case TextureDimension::CubeMap:
            return VK_IMAGE_TYPE_2D; // cubemaps are always 2D images.....unless texture array?
        default:
            assert(false && "Unknown Texture dimension.");
            return VK_IMAGE_TYPE_MAX_ENUM;
        }
    }

    [[nodiscard]] constexpr VkImageViewType ToImgViewType(TextureDimension dimension)
    {
        switch (dimension)
        {
        case TextureDimension::Texture1D:
            return VK_IMAGE_VIEW_TYPE_1D;
        case TextureDimension::Texture2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::Texture3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case TextureDimension::CubeMap:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            assert(false && "Unknown Texture view dimension.");
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }

    [[nodiscard]] constexpr VkImageUsageFlags ToVkImageUsage(ImageUsageFlags usage)
    {
        // Define a mapping between Engine flags and Vulkan flags
        struct UsageMapping {
            ImageUsage engine;
            VkImageUsageFlagBits vk;
        };

        static constexpr UsageMapping table[] = {
            { ImageUsage::TransferSrc,     VK_IMAGE_USAGE_TRANSFER_SRC_BIT },
            { ImageUsage::TransferDst,     VK_IMAGE_USAGE_TRANSFER_DST_BIT },
            { ImageUsage::Sampled,         VK_IMAGE_USAGE_SAMPLED_BIT },
            { ImageUsage::ColorAttachment, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
            { ImageUsage::DepthStencil,    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT },
            { ImageUsage::Storage,         VK_IMAGE_USAGE_STORAGE_BIT },
            { ImageUsage::InputAttachment, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT },
            { ImageUsage::ResolveDst,      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
            { ImageUsage::ResolveSrc,      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
            { ImageUsage::Transient,       VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT }
        };

        VkImageUsageFlags flags = 0;

        // Use a constexpr-friendly loop
        for (const auto& entry : table)
        {
            if (HasAny(usage, entry.engine))
            {
                flags |= entry.vk;
            }
        }

        return flags;
    }


    [[nodiscard]] constexpr VkFormat ToVkFormat(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;

        case TextureFormat::R8_SRGB: return VK_FORMAT_R8_SRGB;
        case TextureFormat::RG8_SRGB: return VK_FORMAT_R8G8_SRGB;
        case TextureFormat::RGB8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
        case TextureFormat::RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::BGRA8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;

        case TextureFormat::R8_UINT: return VK_FORMAT_R8_UINT;
        case TextureFormat::RG8_UINT: return VK_FORMAT_R8G8_UINT;
        case TextureFormat::RGBA8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
        case TextureFormat::R16_UINT: return VK_FORMAT_R16_UINT;
        case TextureFormat::RG16_UINT: return VK_FORMAT_R16G16_UINT;
        case TextureFormat::RGBA16_UINT: return VK_FORMAT_R16G16B16A16_UINT;
        case TextureFormat::R32_UINT: return VK_FORMAT_R32_UINT;
        case TextureFormat::RG32_UINT: return VK_FORMAT_R32G32_UINT;
        case TextureFormat::RGBA32_UINT: return VK_FORMAT_R32G32B32A32_UINT;

        case TextureFormat::R8_SINT: return VK_FORMAT_R8_SINT;
        case TextureFormat::RG8_SINT: return VK_FORMAT_R8G8_SINT;
        case TextureFormat::RGBA8_SINT: return VK_FORMAT_R8G8B8A8_SINT;
        case TextureFormat::R16_SINT: return VK_FORMAT_R16_SINT;
        case TextureFormat::RG16_SINT: return VK_FORMAT_R16G16_SINT;
        case TextureFormat::RGBA16_SINT: return VK_FORMAT_R16G16B16A16_SINT;
        case TextureFormat::R32_SINT: return VK_FORMAT_R32_SINT;
        case TextureFormat::RG32_SINT: return VK_FORMAT_R32G32_SINT;
        case TextureFormat::RGBA32_SINT: return VK_FORMAT_R32G32B32A32_SINT;
        case TextureFormat::R16_SFLOAT: return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::RG16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGB16_SFLOAT: return VK_FORMAT_R16G16B16_SFLOAT;
        case TextureFormat::RGBA16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;

        case TextureFormat::R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGB32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;

        case TextureFormat::R10G10B10A2_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::R11G11B10_UFLOAT: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case TextureFormat::R9G9B9E5_UFLOAT: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

        case TextureFormat::BC1_RGB_UNORM_BLOCK: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case TextureFormat::BC1_RGBA_UNORM_BLOCK: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TextureFormat::BC2_UNORM_BLOCK: return VK_FORMAT_BC2_UNORM_BLOCK;
        case TextureFormat::BC3_UNORM_BLOCK: return VK_FORMAT_BC3_UNORM_BLOCK;
        case TextureFormat::BC4_UNORM_BLOCK: return VK_FORMAT_BC4_UNORM_BLOCK;
        case TextureFormat::BC5_UNORM_BLOCK: return VK_FORMAT_BC5_UNORM_BLOCK;
        case TextureFormat::BC6H_SFLOAT_BLOCK: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case TextureFormat::BC7_UNORM_BLOCK: return VK_FORMAT_BC7_UNORM_BLOCK;
        case TextureFormat::ETC2_RGB8_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case TextureFormat::ETC2_RGBA8_UNORM_BLOCK: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        case TextureFormat::ASTC_4x4_UNORM_BLOCK: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        case TextureFormat::ASTC_8x8_UNORM_BLOCK: return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;

        case TextureFormat::D16_UNORM: return VK_FORMAT_D16_UNORM;
        case TextureFormat::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;

        default:
        case TextureFormat::UNKNOWN: return VK_FORMAT_UNDEFINED;
        }
    }

    [[nodiscard]] constexpr VkImageLayout ToVk(TextureLayout layout)
    {
        switch (layout)
        {
        case TextureLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
        case TextureLayout::ShaderReadOnly: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case TextureLayout::ColorWrite: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case TextureLayout::DepthWrite: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        case TextureLayout::DepthReadOnly: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case TextureLayout::CopySource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case TextureLayout::CopyDestination: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case TextureLayout::ResolveSource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case TextureLayout::ResolveDestination: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case TextureLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default: case TextureLayout::Unknown: return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    [[nodiscard]] constexpr VkSampleCountFlagBits ToVk(SampleCount count)
    {
        switch (count)
        {
        case SampleCount::X1: return VK_SAMPLE_COUNT_1_BIT;
        case SampleCount::X2: return VK_SAMPLE_COUNT_2_BIT;
        case SampleCount::X4: return VK_SAMPLE_COUNT_4_BIT;
        case SampleCount::X8: return VK_SAMPLE_COUNT_8_BIT;
        case SampleCount::X16: return VK_SAMPLE_COUNT_16_BIT;
        case SampleCount::X32: return VK_SAMPLE_COUNT_32_BIT;
        case SampleCount::X64: return VK_SAMPLE_COUNT_64_BIT;
        default: return VK_SAMPLE_COUNT_1_BIT;
        }
    }

    [[nodiscard]] constexpr bool IsDepthFormat(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr VkComponentSwizzle ToVk(TextureSwizzle s)
    {
        switch (s)
        {
        case TextureSwizzle::Identity: return VK_COMPONENT_SWIZZLE_IDENTITY;
        case TextureSwizzle::Zero: return VK_COMPONENT_SWIZZLE_ZERO;
        case TextureSwizzle::One: return VK_COMPONENT_SWIZZLE_ONE;
        case TextureSwizzle::R: return VK_COMPONENT_SWIZZLE_R;
        case TextureSwizzle::G: return VK_COMPONENT_SWIZZLE_G;
        case TextureSwizzle::B: return VK_COMPONENT_SWIZZLE_B;
        case TextureSwizzle::A: return VK_COMPONENT_SWIZZLE_A;
        }
        return VK_COMPONENT_SWIZZLE_IDENTITY;
    }

    [[nodiscard]] constexpr u32 BytesPerTexel(TextureFormat fmt)
    {
        switch (fmt)
        {
        case TextureFormat::R32_SFLOAT: return 4u;
        case TextureFormat::RG32_SFLOAT: return 8u;
        case TextureFormat::RGB32_SFLOAT: return 12u;
        case TextureFormat::RGBA32_SFLOAT: return 16u;


        case TextureFormat::R16_SFLOAT: return 2u;
        case TextureFormat::RG16_SFLOAT: return 4u;
        case TextureFormat::RGBA16_SFLOAT: return 8u;


        case TextureFormat::RGBA8_UNORM:
        case TextureFormat::RGBA8_SRGB:
        case TextureFormat::BGRA8_UNORM:
        case TextureFormat::BGRA8_SRGB:
            return 4u;
        case TextureFormat::D32_SFLOAT: return 4u;

        default:
            assert(false && "BytesPerTexel: unsupported format");
            return 0;
        }
    }

    [[nodiscard]] constexpr VkFilter ToVk(SamplerFilter filter)
    {
        return filter == SamplerFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    }

    [[nodiscard]] constexpr VkSamplerMipmapMode ToVk(SamplerMipFilter mip)
    {
        switch (mip)
        {
        case SamplerMipFilter::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case SamplerMipFilter::Nearest:
        default: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }
    }

    [[nodiscard]] constexpr VkSamplerAddressMode ToVk(SamplerAddressMode mode)
    {
        switch (mode)
        {
        case SamplerAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    [[nodiscard]] constexpr VkBorderColor ToVk(SamplerBorderColor color)
    {
        switch (color)
        {
        case SamplerBorderColor::FloatTransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case SamplerBorderColor::FloatOpaqueBlack: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case SamplerBorderColor::FloatOpaqueWhite: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        default: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        }
    }

    // Descriptor
    [[nodiscard]] constexpr VkDescriptorType ToVk(DescriptorType type)
    {
        switch (type)
        {
        case DescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::SampledImage: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::InputAttachment: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    // Vertex description
    [[nodiscard]] constexpr VkVertexInputRate ToVk(VertexInputRate rate)
    {
        return (rate == VertexInputRate::Instance)
                   ? VK_VERTEX_INPUT_RATE_INSTANCE
                   : VK_VERTEX_INPUT_RATE_VERTEX;
    }

   [[nodiscard]] constexpr VkVertexInputBindingDescription ToVk(const VertexBindingDescription& desc)
    {
        return {
            .binding = desc.binding,
            .stride = desc.stride,
            .inputRate = ToVk(desc.inputRate)
        };
    }

    [[nodiscard]] constexpr VkVertexInputAttributeDescription ToVk(const VertexAttributeDescription& desc)
    {
        return {
            .location = desc.location,
            .binding = desc.binding,
            .format = ToVkFormat(desc.format),
            .offset = desc.offset
        };
    }


    [[nodiscard]] constexpr VkSpirvResourceTypeFlagsEXT ToSpirvType(DescriptorType type)
    {
        switch (type)
        {
        case DescriptorType::Sampler:               return VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT;
        case DescriptorType::SampledImage:          return VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT;
        case DescriptorType::StorageImage:          return VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
        case DescriptorType::UniformBuffer:         return VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
        case DescriptorType::StorageBuffer:         return VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
        case DescriptorType::CombinedImageSampler:  return VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT;

        default: return 0;
        }
    }

} // namespace Renderer
