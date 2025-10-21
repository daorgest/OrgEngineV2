//
// Created by Orgest on 8/3/2025.
//

#include "VulkanMesh.h"

#include <algorithm>
#include <unordered_set>


#include "MathFuncs.h"
#include "MeshLoader.h"
#include "RendererTypes.h"
#include "TextureManager.h"
#include "Tools/Arena.h"
#include "Tools/Array.h"
#include "Tools/Timer.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

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

	Array<DescriptorAllocatorGrowable::PoolSizeRatio, 4> sizes = {
		{DescriptorType::SampledImage, 3},
		{DescriptorType::Sampler, 3},
		{DescriptorType::UniformBuffer, 3},
		{DescriptorType::StorageBuffer, 1}
	};

	const u32 setsHint = std::max<u32>(1, static_cast<u32>(loadedModel.materials.size()));
	descriptorPool.Init(device, setsHint, sizes);

	images.reserve(images.size() + loadedModel.materials.size() * 2); // albedo + normal image path deduplication

	// path -> (image,sampler)
	std::unordered_map<std::string, TextureOrFallback> loadedTextures;
	loadedTextures.reserve(loadedModel.materials.size());

	// Texture deduplication
	auto loadTex = [&](const std::string& path, bool srgb) -> std::optional<TextureOrFallback>
	{
		TIME_BLOCK("Load Texture");
		if (path.empty()) return std::nullopt;

		// Dedup check
		if (const auto it = loadedTextures.find(path); it != loadedTextures.end())
			return it->second;

		auto texData = TextureManager::LoadTextureFromSTB(path, srgb);
		if (!texData) return std::nullopt;

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
			imgRef.UploadTextureToGPU(texData->data.data(), texInfo);
		}

		TextureOrFallback handle{ &imgRef, sharedSampler };
		loadedTextures.emplace(path, handle);
		return handle;
	};

	// Material layout
	{
		TIME_BLOCK("Build Material Descriptor Layout");
		DescriptorLayoutBuilder builder;
		builder.AddBinding(0, DescriptorType::SampledImage);
		builder.AddBinding(1, DescriptorType::Sampler);
		builder.AddBinding(2, DescriptorType::SampledImage);
		layout = builder.Build(device->device, ShaderStage::Fragment);
	}

	materials.reserve(loadedModel.materials.size());

	{
		TIME_BLOCK("Create Materials");

		DescriptorWriter w;
		for (const auto& cpuMat : loadedModel.materials)
		{
			TextureOrFallback albedo = loadTex(cpuMat.albedoPath, true).value_or(fallback);;
			TextureOrFallback normal = loadTex(cpuMat.normalPath, false).value_or(fallback);

			DescriptorSet set = descriptorPool.Allocate(layout);
			{
				TIME_BLOCK("Write Descriptor");
				w.Clear();
				w.WriteImage(0, albedo.fallbackImage, nullptr, DescriptorType::SampledImage); // t0
				w.WriteImage(1, std::nullopt, albedo.fallbackSampler, DescriptorType::Sampler); // s0
				w.WriteImage(2, normal.fallbackImage, nullptr, DescriptorType::SampledImage); // t1
				w.UpdateSet(device->device, set.vk);
			}

			VulkanMaterial mat{};
			mat.colorImage    = albedo.fallbackImage;
			mat.sampler       = albedo.fallbackSampler;
			mat.normalImage   = normal.fallbackImage;
			mat.descriptorSet = set;

			materials.emplace_back(mat);
		}
	}

	// Build mesh parts - BATCHED GPU UPLOAD
	{
		TIME_BLOCK("Create Mesh Parts");
		
		// Count total parts and calculate sizes
		size_t totalParts = 0;
		size_t totalVertexBytes = 0;
		size_t totalIndexBytes = 0;
		
		for (auto& mesh : loadedModel.meshes)
		{
			for (const auto& part : mesh.parts)
			{
				totalParts++;
				totalVertexBytes += part.vertices.size() * sizeof(Vertex);
				totalIndexBytes += part.indices.size() * sizeof(u32);
			}
		}
		
		parts.reserve(totalParts);
		
		const size_t totalStagingSize = totalVertexBytes + totalIndexBytes;
		VulkanBuffer stagingBuffer(device, BufferPreset::StagingUpload, totalStagingSize);
		
		// Temporary structure to track offsets during batching
		struct PartUploadInfo {
			size_t stagingOffset;
			size_t vertexSize;
			size_t indexSize;
		};
		Vector<PartUploadInfo> uploadInfos;
		uploadInfos.reserve(totalParts);
		
		size_t stagingOffset = 0;
		
		// First pass: Create buffers and copy to staging
		{
			TIME_BLOCK("Prepare Buffers & Copy to Staging");
			for (auto& mesh : loadedModel.meshes)
			{
				for (const auto& part : mesh.parts)
				{
					VulkanMeshPart gpuPart;
					gpuPart.materialIndex = part.materialIndex;
					gpuPart.indexCount = static_cast<u32>(part.indices.size());
					gpuPart.localBounds = part.aabb;  // Keep AABB
					gpuPart.transform = glm::mat4(1.0f);  // Identity transform
					
					const size_t vertexSize = part.vertices.size() * sizeof(Vertex);
					const size_t indexSize = part.indices.size() * sizeof(u32);
					
					// Create GPU buffers
					gpuPart.vertexBuffer.Init(device, BufferPreset::VertexStorageGPU, vertexSize);
					gpuPart.vertexAddress = gpuPart.vertexBuffer.GetDeviceAddress();
					gpuPart.indexBuffer.Init(device, BufferPreset::IndexGPU, indexSize);
					
					// Copy to staging buffer at current offset
					vmaCopyMemoryToAllocation(device->allocator, part.vertices.data(), 
						stagingBuffer.allocation, stagingOffset, vertexSize);
					vmaCopyMemoryToAllocation(device->allocator, part.indices.data(), 
						stagingBuffer.allocation, stagingOffset + vertexSize, indexSize);
					
					// Store upload info separately
					uploadInfos.push_back({stagingOffset, vertexSize, indexSize});
					stagingOffset += vertexSize + indexSize;
					
					parts.emplace_back(std::move(gpuPart));
				}
			}
		}
		
		// Second pass: SINGLE batched GPU upload
		{
			TIME_BLOCK("Batched GPU Upload");
			device->ImmediateSubmit([&](VkCommandBuffer cmd)
			{
				for (size_t i = 0; i < parts.size(); ++i)
				{
					auto& part = parts[i];
					const auto& info = uploadInfos[i];
					
					// Copy from staging to GPU buffers
					part.vertexBuffer.CopyFrom(cmd, &stagingBuffer, info.vertexSize, info.stagingOffset, 0);
					part.indexBuffer.CopyFrom(cmd, &stagingBuffer, info.indexSize, info.stagingOffset + info.vertexSize, 0);
				}
			});
		}
		
		// Clean up staging buffer
		stagingBuffer.Destroy();
	}
	
	std::ranges::sort(parts,
	                  [](const VulkanMeshPart& a, const VulkanMeshPart& b)
	                  { return a.materialIndex < b.materialIndex; });


}

void VulkanModel::Destroy(const VulkanDevice* device)
{
	descriptorPool.DestroyPools();

	for (auto& part : parts) part.Destroy();
	parts.clear();

	for (auto& img : images) img.Destroy();
	images.clear();

	for (auto& s : samplers) s.Destroy();
	samplers.clear();

	vkDestroyDescriptorSetLayout(device->device, layout, nullptr);
}

void VulkanMeshPart::Destroy()
{
	indexBuffer.Destroy();
	vertexBuffer.Destroy();
}

bool VulkanMeshPart::Create(VulkanDevice* device, const MeshPart& mesh)
{
	materialIndex = mesh.materialIndex;
	indexCount = static_cast<u32>(mesh.indices.size());

	const size_t vertexSize = mesh.vertices.size() * sizeof(Vertex);
	const size_t indexSize = mesh.indices.size() * sizeof(u32);
	const size_t totalSize = vertexSize + indexSize;

	vertexBuffer.Init(device, BufferPreset::VertexStorageGPU, vertexSize);
	vertexAddress = vertexBuffer.GetDeviceAddress();

	indexBuffer.Init(device, BufferPreset::IndexGPU, indexSize);

	VulkanBuffer stagingBuffer(device, BufferPreset::StagingUpload, totalSize);

	vmaCopyMemoryToAllocation(device->allocator, mesh.vertices.data(), stagingBuffer.allocation, 0, vertexSize);
	vmaCopyMemoryToAllocation(device->allocator, mesh.indices.data(), stagingBuffer.allocation, vertexSize, indexSize);

	device->ImmediateSubmit([&](VkCommandBuffer cmd)
	{
		vertexBuffer.CopyFrom(cmd, &stagingBuffer, vertexSize, 0, 0);
		indexBuffer.CopyFrom(cmd, &stagingBuffer, indexSize, vertexSize, 0);
	});

	localBounds = mesh.aabb;

	stagingBuffer.Destroy();

	return true;
}


void VulkanMeshPart::Draw(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, DescriptorSet materialSet) const
{
	assert(indexCount > 0);
	assert(indexBuffer.IsValid() && "indexBuffer is invalid");
	assert(vertexBuffer.IsValid() && "vertexBuffer is invalid");

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &materialSet.vk, 0, nullptr);
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

