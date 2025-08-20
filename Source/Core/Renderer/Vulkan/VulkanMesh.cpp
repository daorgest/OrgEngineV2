//
// Created by Orgest on 8/3/2025.
//

#include "VulkanMesh.h"

#include <algorithm>
#include <unordered_set>


#include "Arena.h"
#include "MeshLoader.h"
#include "RendererTypes.h"
#include "TextureManager.h"
#include "Array.h"
#include "MathFuncs.h"
#include "Timer.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanInit.h"
#include "VulkanTexture.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

void VulkanModel::LoadModel(VulkanDevice* device, const LoadedModel& loadedModel, TextureFallback* fallback)
{
	TIME_FUNCTION(); // Full LoadModel() time
	assert(fallbackImage && "You must provide a fallbackImage to VulkanModel::LoadModel");

	// Shared sampler
	{
		TIME_BLOCK("Create Shared Sampler");
		SamplerDesc samplerDesc{};
		samplerDesc.minFilter = SamplerFilter::Linear;
		samplerDesc.magFilter = SamplerFilter::Linear;
		samplerDesc.mipFilter = SamplerMipFilter::Linear;

		this->samplers.emplace_back(device, samplerDesc);
	}

	VulkanSampler* sharedSampler = &this->samplers[0];

	Array<DescriptorAllocatorGrowable::PoolSizeRatio, 4> sizes = {
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3.f},
		{VK_DESCRIPTOR_TYPE_SAMPLER, 3.f},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3.f},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.f}
	};

	const u32 setsHint = std::max<u32>(1, static_cast<u32>(loadedModel.materials.size()));
	descriptorPool.Init(device, setsHint, sizes);

	// Reserve image storage + dedup map (keep pointers stable)
	std::unordered_set<std::string> uniquePaths;
	uniquePaths.reserve(loadedModel.materials.size());
	for (const auto& m : loadedModel.materials)
		if (!m.albedoPath.empty()) uniquePaths.insert(m.albedoPath);
	images.reserve(images.size() + uniquePaths.size());

	// path -> (image,sampler)
	std::unordered_map<std::string, TextureFallback> loadedTextures;
	loadedTextures.reserve(uniquePaths.size());

	// Texture deduplication
	auto loadTex = [&](const std::string& path) -> TextureFallback
	{
		TIME_BLOCK("Load Texture");
		if (path.empty()) return *fallback;

		// Dedup check
		if (auto it = loadedTextures.find(path); it != loadedTextures.end())
			return it->second;

		auto texData = TextureManager::LoadTextureFromSTB(path, true);
		if (!texData) return *fallback;

		TextureInfo texInfo{
			.extent    = { (u32)texData->width, (u32)texData->height, 1 },
			.mipLevels = 1,
			.type      = ImageType::Image2D,
			.format    = texData->format,
			.dimension = TextureDimension::TEXTURE_2D,
			.usage     = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST | ImageUsage::TRANSFER_SRC
		};
		texInfo.EnableMipmaps();

		// Push by value (stack allocation inside vector)
		images.emplace_back(device, texInfo);
		VulkanImage& imgRef = images.back();

		{
			TIME_BLOCK("Upload Texture to GPU");
			imgRef.UploadTextureToGPU(texData->data.data(), texInfo);
		}

		TextureFallback handle{ &imgRef, sharedSampler };
		loadedTextures.emplace(path, handle);
		return handle;
	};

	// Material layout
	{
		TIME_BLOCK("Build Material Descriptor Layout");
		DescriptorLayoutBuilder builder;
		builder.AddBinding(0, DescriptorType::SampledImage);
		builder.AddBinding(1, DescriptorType::Sampler);
		this->layout = builder.Build(device->device, ShaderStage::FRAGMENT);
	}

	materials.reserve(materials.size() + loadedModel.materials.size());

	{
		TIME_BLOCK("Create Materials");
		for (const auto& cpuMat : loadedModel.materials)
		{
			TextureFallback albedo = loadTex(cpuMat.albedoPath);

			VkDescriptorSet set = descriptorPool.Allocate(layout);

			{
				TIME_BLOCK("Write Descriptor");
				VkDescriptorWriter w;
				w.WriteImage(0, albedo.fallbackImage,  nullptr,           DescriptorType::SampledImage); // t0
				w.WriteImage(1, std::nullopt,  albedo.fallbackSampler,     DescriptorType::Sampler);      // s0
				w.UpdateSet(device->device, set);
			}

			VulkanMaterial mat{};
			mat.colorImage    = albedo.fallbackImage;
			mat.sampler       = albedo.fallbackSampler;
			mat.materialIndex = static_cast<u32>(materials.size());
			mat.descriptorSet = set;

			materials.push_back(std::move(mat));
		}
	}

	// Build mesh parts
	{
		TIME_BLOCK("Create Mesh Parts");
		for (const auto& mesh : loadedModel.meshes)
		{
			for (const auto& part : mesh.parts)
			{
				VulkanMeshPart gpuPart;
				gpuPart.materialIndex = part.materialIndex;

				{
					TIME_BLOCK("Create VulkanMeshPart");
					gpuPart.Create(device, part);
				}

				parts.push_back(std::move(gpuPart));
			}
		}
	}
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
	indexCount    = static_cast<u32>(mesh.indices.size());

	const size_t vertexSize = mesh.vertices.size() * sizeof(Vertex);
	const size_t indexSize  = mesh.indices.size() * sizeof(u32);
	const size_t totalSize  = vertexSize + indexSize;


	vertexBuffer.Init(device, BufferPreset::VertexStorageGPU, vertexSize);
	indexBuffer.Init(device, BufferPreset::IndexGPU,         indexSize);

	VulkanBuffer stagingBuffer(device, BufferPreset::StagingUpload, totalSize);

	vmaCopyMemoryToAllocation(device->allocator, mesh.vertices.data(), stagingBuffer.allocation, 0, vertexSize);
	vmaCopyMemoryToAllocation(device->allocator, mesh.indices.data(), stagingBuffer.allocation, vertexSize, indexSize);

	device->ImmediateSubmit([&](VkCommandBuffer cmd)
	{
		vertexBuffer.CopyFrom(cmd, &stagingBuffer, vertexSize, 0, 0);
		indexBuffer.CopyFrom(cmd, &stagingBuffer, indexSize, vertexSize, 0);
	});

	stagingBuffer.Destroy();

	// Cache device address
	vertexAddress = vertexBuffer.GetDeviceAddress();

	return true;
}


void VulkanMeshPart::Draw(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, VkDescriptorSet materialSet) const
{
	assert(indexCount > 0);
	assert(indexBuffer.IsValid() && "indexBuffer is invalid");
	assert(vertexBuffer.IsValid() && "vertexBuffer is invalid");

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &materialSet, 0, nullptr);
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

