//
// Created by Orgest on 6/11/2025.
//

#pragma once
#include <cassert>
#include <span>
#include <type_traits>
#include <volk.h>

#include "RendererTypes.h"

namespace Renderer
{
	// Enum Utils
	template <typename E>
	constexpr bool HasAndFlag(E value, E flag)
	{
		using UT = std::underlying_type_t<E>;
		return (static_cast<UT>(value) & static_cast<UT>(flag)) != 0;
	}

	template <typename E>
	constexpr bool HasOrFlag(E value, E flag)
	{
		using UT = std::underlying_type_t<E>;
		return (static_cast<UT>(value) | static_cast<UT>(flag)) != 0;
	}

	// Shader Stage conversions
	inline VkShaderStageFlags ToVk(ShaderStageFlags flags)
	{
		VkShaderStageFlags vkFlags = 0;

		if (flags & ShaderStage::VERTEX)         vkFlags |= VK_SHADER_STAGE_VERTEX_BIT;
		if (flags & ShaderStage::FRAGMENT)       vkFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (flags & ShaderStage::COMPUTE)        vkFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
		if (flags & ShaderStage::RAYGEN)         vkFlags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		if (flags & ShaderStage::ANY_HIT)        vkFlags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		if (flags & ShaderStage::CLOSEST_HIT)    vkFlags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		if (flags & ShaderStage::MISS)           vkFlags |= VK_SHADER_STAGE_MISS_BIT_KHR;
		if (flags & ShaderStage::CALLABLE)       vkFlags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

		return vkFlags;
	}


	// Present mode conversions
	inline VkPresentModeKHR ToVkPresentMode(PresentMode mode, std::span<const VkPresentModeKHR> available)
	{

		auto has = [&](VkPresentModeKHR m) {
			return std::ranges::find(available, m) != available.end();
		};

		switch (mode)
		{
		case PresentMode::VSyncOn:
			return VK_PRESENT_MODE_FIFO_KHR; // always available

		case PresentMode::LowLatency: // Tear-free, low latency
			if (has(VK_PRESENT_MODE_MAILBOX_KHR)) return VK_PRESENT_MODE_MAILBOX_KHR;
			return VK_PRESENT_MODE_FIFO_KHR;

		case PresentMode::VSyncOff:
			if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) return VK_PRESENT_MODE_IMMEDIATE_KHR;
			if (has(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			return VK_PRESENT_MODE_FIFO_KHR;

		case PresentMode::RelaxedVSync: // Adaptive VSync
			if (has(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			return VK_PRESENT_MODE_FIFO_KHR;

		default:
			return VK_PRESENT_MODE_FIFO_KHR;
		}
	}
	// Images
	inline VkImageType ToImgType(TextureDimension dimension)
	{
		switch (dimension)
		{
		case TextureDimension::TEXTURE_1D:
			return VK_IMAGE_TYPE_1D;
		case TextureDimension::TEXTURE_2D:
			return VK_IMAGE_TYPE_2D;
		case TextureDimension::TEXTURE_3D:
			return VK_IMAGE_TYPE_3D;
		default:
			assert(false && "Unknown Texture dimension.");
			return VK_IMAGE_TYPE_MAX_ENUM;
		}
	}

	inline VkImageViewType ToImgViewType(TextureDimension dimension)
	{
		switch (dimension)
		{
		case TextureDimension::TEXTURE_1D:
			return VK_IMAGE_VIEW_TYPE_1D;
		case TextureDimension::TEXTURE_2D:
			return VK_IMAGE_VIEW_TYPE_2D;
		case TextureDimension::TEXTURE_3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		default:
			assert(false && "Unknown Texture view dimension.");
			return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
		}
	}

	inline VkImageUsageFlags ToVkImageUsage(ImageUsageFlags usage)
	{
		VkImageUsageFlags flags = 0;

		if (usage & ImageUsage::TRANSFER_SRC)
			flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		if (usage & ImageUsage::TRANSFER_DST)
			flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		if (usage & ImageUsage::SAMPLED)
			flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

		if (usage & ImageUsage::COLOR_ATTACHMENT)
			flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (usage & ImageUsage::DEPTH_STENCIL_ATTACHMENT)
			flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if (usage & ImageUsage::STORAGE)
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;

		if (usage & ImageUsage::INPUT_ATTACHMENT)
			flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		if (usage & ImageUsage::RESOLVE_DST)
			flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		if (usage & ImageUsage::RESOLVE_SRC)
			flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		return flags;
	}

	inline VkFormat ToVkFormat(TextureFormat format)
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

		case TextureFormat::R16_SFLOAT: return VK_FORMAT_R16_SFLOAT;
		case TextureFormat::RG16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
		case TextureFormat::RGBA16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case TextureFormat::R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
		case TextureFormat::RG32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
		case TextureFormat::RGB32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
		case TextureFormat::RGBA32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;

		case TextureFormat::BC1_RGB_UNORM_BLOCK: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case TextureFormat::BC1_RGBA_UNORM_BLOCK: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case TextureFormat::BC2_UNORM_BLOCK: return VK_FORMAT_BC2_UNORM_BLOCK;
		case TextureFormat::BC3_UNORM_BLOCK: return VK_FORMAT_BC3_UNORM_BLOCK;
		case TextureFormat::BC4_UNORM_BLOCK: return VK_FORMAT_BC4_UNORM_BLOCK;
		case TextureFormat::BC5_UNORM_BLOCK: return VK_FORMAT_BC5_UNORM_BLOCK;
		case TextureFormat::BC6H_SFLOAT_BLOCK: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case TextureFormat::BC7_UNORM_BLOCK: return VK_FORMAT_BC7_UNORM_BLOCK;

		case TextureFormat::D16_UNORM: return VK_FORMAT_D16_UNORM;
		case TextureFormat::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
		case TextureFormat::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
		case TextureFormat::D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;

		default: case TextureFormat::IMAGE_FORMAT_UNKNOWN: return VK_FORMAT_UNDEFINED;
		}
	}

	inline VkImageLayout ToVk(TextureLayout layout)
	{
		switch (layout)
		{
		case TextureLayout::General:              return VK_IMAGE_LAYOUT_GENERAL;
		case TextureLayout::ShaderReadOnly:       return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case TextureLayout::ColorWrite:           return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case TextureLayout::DepthWrite:           return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		case TextureLayout::DepthReadOnly:        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		case TextureLayout::CopySource:           return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case TextureLayout::CopyDestination:      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case TextureLayout::ResolveSource:        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case TextureLayout::ResolveDestination:   return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case TextureLayout::Present:              return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		default: case TextureLayout::Unknown:     return VK_IMAGE_LAYOUT_UNDEFINED;
		}
	}

	inline bool IsDepthFormat(VkFormat format)
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

	inline VkFilter ToVk(SamplerFilter filter)
	{
		return filter == SamplerFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	}

	inline VkSamplerMipmapMode ToVk(SamplerMipFilter mip)
	{
		switch (mip)
		{
		case SamplerMipFilter::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case SamplerMipFilter::Nearest:
		default: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		}
	}

	inline VkSamplerAddressMode ToVk(SamplerAddressMode mode)
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

	inline VkBorderColor ToVk(SamplerBorderColor color)
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
	inline VkDescriptorType ToVk(DescriptorType type)
	{
		switch (type)
		{
		case DescriptorType::Sampler:              return VK_DESCRIPTOR_TYPE_SAMPLER;
		case DescriptorType::SampledImage:         return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case DescriptorType::StorageImage:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case DescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case DescriptorType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case DescriptorType::InputAttachment:      return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		default:                                   return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	}

}
