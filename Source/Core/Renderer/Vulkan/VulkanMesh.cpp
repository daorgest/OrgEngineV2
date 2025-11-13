//
// Created by Orgest on 8/3/2025.
//

#include "VulkanMesh.h"

#include <algorithm>
#include <unordered_set>

#include "RendererTypes.h"
#include "../../../Engine/MeshLoader.h"
#include "../../../Engine/TextureManager.h"
#include "Tools/Array.h"
#include "Tools/Timer.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"
#include "Tools/DeletionQueue.h"

using Renderer::VulkanImage;
using Renderer::VulkanDevice;
using Renderer::VulkanBuffer;
using Renderer::VulkanModel;
using Renderer::DescriptorLayout;

VulkanModel::VulkanModel(VulkanDevice* device, LoadedModel& loadedModel, TextureOrFallback& fallback)
{
	TIME_FUNCTION(); // Full LoadModel() time
	assert(fallback.fallbackImage && fallback.fallbackSampler && fallback.fallbackNormalImage &&
		"Global fallbacks must be initialized before loading models");

	// Shared sampler
	{
		TIME_BLOCK("Create Shared Sampler");
		SamplerDesc samplerDesc = {
			.minFilter = SamplerFilter::Linear,
			.magFilter = SamplerFilter::Linear,
			.mipFilter = SamplerMipFilter::Linear
		};
		samplers.emplace_back(device, samplerDesc);
	}

	VulkanSampler* sharedSampler = samplers.data();


	const u32 materialCount = static_cast<u32>(loadedModel.materials.size());
	const u32 setsHint = std::max<u32>(1, materialCount);
	Array<DescriptorAllocatorGrowable::PoolSizeRatio, 3> poolSizes = {
		{ DescriptorType::CombinedImageSampler, 6 }, // albedo + normal per material
		{ DescriptorType::UniformBuffer, 3 },
		{ DescriptorType::StorageBuffer, 1 }
	};

	descriptorPool.Init(device, setsHint, poolSizes);

	// Material layout
	DescriptorLayoutBuilder builder;
	builder.AddBinding(0, DescriptorType::CombinedImageSampler); // albedo
	builder.AddBinding(1, DescriptorType::CombinedImageSampler); // normal
	layout = builder.Build(device->device, ShaderStage::Fragment);


	// path -> (image,sampler)
	std::unordered_map<std::string, TextureOrFallback> textureCache;
	textureCache.reserve(builder.bindings.size());

	// Texture deduplication
	auto loadTex = [&](const std::string& path, const bool srgb) -> TextureOrFallback
	{
		TIME_BLOCK("Load Texture");
		if (path.empty()) return fallback;

		// Fast path: dedupe + insert placeholder in one step
		auto [it, inserted] = textureCache.try_emplace(path, TextureOrFallback{});

		if (!inserted)
		{
			// Already in cache
			return it->second;
		}

		auto texData = TextureManager::LoadTextureFromSTB(path, srgb);
		if (!texData)
		{
			it->second = fallback;   // Mark cache entry as fallback if load fails
			return fallback;
		}

		TextureInfo texInfo{
			.extent    = { static_cast<u32>(texData->width), static_cast<u32>(texData->height), 1 },
			.mipLevels = 1,
			.type      = ImageType::Image2D,
			.format    = texData->format,
			.dimension = TextureDimension::Texture2D,
			.usage     = ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc
		};
		texInfo.EnableMipmaps();

		images.emplace_back(device, texInfo);
		VulkanImage& imgRef = images.back();

		{
			TIME_BLOCK("Upload Texture to GPU");
			imgRef.UploadTextureToGPU(std::get<Vector<u8>>(texData->data).data(), texInfo);
		}

		// Store the constructed texture in the cache entry
		it->second = { &imgRef, sharedSampler };
		return it->second;
	};

	{
		TIME_BLOCK("Create Materials");
		materials.reserve(materialCount);
		DescriptorWriter writer;
		for (const auto& cpuMat : loadedModel.materials)
		{
			TextureOrFallback albedo = loadTex(cpuMat.albedoPath, true);
			TextureOrFallback normal = loadTex(cpuMat.normalPath, false);

			DescriptorSet set = descriptorPool.Allocate(layout);
			writer.Clear();
			writer.WriteCombinedImage(0, albedo.fallbackImage, albedo.fallbackSampler);
			writer.WriteCombinedImage(1, normal.fallbackImage, normal.fallbackSampler);
			writer.UpdateSet(device->device, set.vk);

			VulkanMaterial mat = {
				.colorImage    = albedo.fallbackImage,
				.sampler       = albedo.fallbackSampler,
				.normalImage   = normal.fallbackImage,
				.descriptorSet = set,
				.baseColor     = cpuMat.baseColor,
				.roughness     = cpuMat.roughness,
				.metallic      = cpuMat.metallic,
				.ior           = cpuMat.ior,
				.opacity       = cpuMat.opacity,
				.emissive      = cpuMat.emissive
			};

			materials.emplace_back(mat);
		}
	}

	// Build ONE vertex buffer and ONE index buffer for the whole model
	{
		TIME_BLOCK("Create Unified Mesh Buffers");

		size_t totalVertices = 0;
		size_t totalIndices = 0;
		size_t totalParts = 0;

		for (auto& mesh : loadedModel.meshes)
		{
			totalVertices += mesh.unifiedVertices.size();
			totalIndices += mesh.unifiedIndices.size();
			totalParts += mesh.parts.size();
		}

		if (totalVertices == 0 || totalIndices == 0)
		{
			LOG(Warning, "Loaded model has no vertices or indices.");
			return;
		}

		parts.reserve(totalParts);

		const size_t vertexBytes = totalVertices * sizeof(Vertex);
		const size_t indexBytes = totalIndices * sizeof(u32);
		const size_t stagingBytes = vertexBytes + indexBytes;

		vertexBuffer.Init(device, BufferPreset::VertexStorageGPU, vertexBytes);
		indexBuffer.Init(device, BufferPreset::IndexGPU, indexBytes);
		vertexAddress = vertexBuffer.GetDeviceAddress();

		VulkanBuffer staging(device, BufferPreset::StagingUpload, stagingBytes);

		size_t vOffset = 0;
		size_t iOffset = vertexBytes;
		size_t currentVertexCount = 0;
		size_t currentIndexCount = 0;

		for (auto& mesh : loadedModel.meshes)
		{
			if (mesh.unifiedVertices.empty() || mesh.unifiedIndices.empty())
			{
				continue;
			}

			const size_t vb = mesh.unifiedVertices.size() * sizeof(Vertex);
			const size_t ib = mesh.unifiedIndices.size() * sizeof(u32);

			vmaCopyMemoryToAllocation(device->allocator, mesh.unifiedVertices.data(), staging.allocation, vOffset, vb);
			vmaCopyMemoryToAllocation(device->allocator, mesh.unifiedIndices.data(), staging.allocation, iOffset, ib);

			for (const auto& part : mesh.parts)
			{
				parts.emplace_back(VulkanMeshPart{
					.localBounds = part.aabb,
					.materialIndex = part.materialIndex,
					.indexCount = part.indexCount,
					.firstIndex = part.firstIndex + static_cast<u32>(currentIndexCount),
					.vertexOffset = part.vertexOffset + static_cast<u32>(currentVertexCount)
				});
			}

			vOffset += vb;
			iOffset += ib;
			currentVertexCount += mesh.unifiedVertices.size();
			currentIndexCount += mesh.unifiedIndices.size();
		}

		device->immediateSubmitter.Submit([&](VkCommandBuffer cmd)
		{
			vertexBuffer.CopyFrom(cmd, &staging, vertexBytes, 0, 0);
			indexBuffer.CopyFrom(cmd, &staging, indexBytes, vertexBytes, 0);
		});
	}

    // Sort parts by material for efficient rendering
	std::ranges::sort(parts,[](const VulkanMeshPart& a, const VulkanMeshPart& b) -> bool
		{ return a.materialIndex < b.materialIndex; });

	gDeletionQueue.Push([this, device](){this->Destroy(device);}, "VulkanModel");
}

void VulkanModel::Destroy(const VulkanDevice* device)
{
	parts.clear();

	// Destroy the single, unified buffers
	if(indexBuffer.IsValid()) indexBuffer.Destroy();
	if(vertexBuffer.IsValid()) vertexBuffer.Destroy();


	for (auto& img : images) img.Destroy();
	images.clear();

	for (auto& s : samplers) s.Destroy();
	samplers.clear();

	descriptorPool.DestroyPools();

	layout.Destroy(device);
}