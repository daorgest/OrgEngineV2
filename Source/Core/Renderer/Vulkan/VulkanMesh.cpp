//
// Created by Orgest on 8/3/2025.
//

#include "VulkanMesh.h"

#include <algorithm>
#include <unordered_set>

#include "DefaultTextures.h"
#include "RendererTypes.h"
#include "../../../Engine/ShaderConstants.h"
#include "../../../Engine/MeshLoader.h"
#include "../../../Engine/TextureLoader.h"
#include "Tools/Array.h"
#include "Tools/Timer.h"

#include "VulkanBuffer.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "Tools/DeletionQueue.h"

using Renderer::VulkanTexture;
using Renderer::VulkanDevice;
using Renderer::VulkanBuffer;
using Renderer::VulkanModel;
using Renderer::DescriptorLayout;

VulkanModel::VulkanModel(VulkanDevice* device, LoadedModel& loadedModel, DescriptorAllocatorGrowable& globalAllocator,
					TextureDefaults& defaults)
{
	TIME_FUNCTION(); // Full LoadModel() time

	DescriptorSetLayoutDesc matLayoutDesc = {
		.setIndex = 1,
		.bindings = Constants::Material
	};

	materialBuffer = std::make_unique<VulkanShaderBuffer>(device, &globalAllocator, matLayoutDesc);
	materialBuffer->AllocateDescriptorSets(true, 1000);

	std::unordered_map<std::string, std::pair<GPUTexture*, GPUSampler*>> textureCache;
	Vector<std::pair<GPUTexture*, GPUSampler*>> bindlessEntries;

	auto loadTex = [&](const std::string& path, const bool srgb) -> std::pair<GPUTexture*, GPUSampler*>
	{
		if (path.empty()) {
			GPUTexture* fallback = srgb ? defaults.white.get() : defaults.normal.get();
			return { fallback, defaults.linearSampler.get() };
		}

		auto [it, inserted] = textureCache.try_emplace(path);
		if (!inserted) return it->second;

		auto texData = TextureLoader::LoadTextureFromSTB(path, srgb);
		if (!texData) {
			it->second = { defaults.checkerboard.get(), defaults.pointSampler.get() };
			return it->second;
		}

		TextureInfo texInfo{
			.extent    = { static_cast<u32>(texData->width), static_cast<u32>(texData->height), 1 },
			.format    = texData->format,
			.usage     = ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc
		};
		texInfo.EnableMipmaps();

		images.emplace_back(device, texInfo);
		VulkanTexture& imgRef = images.back();
		imgRef.UploadTextureToGPU(std::get<Vector<u8>>(texData->data).data(), texInfo);

		it->second = { &imgRef, defaults.linearSampler.get() };
		return it->second;
	};

	// 3. Helper to register into the local bindless list
	auto getBindlessIndex = [&](const std::string& path, bool srgb) -> u32 {
		auto res = loadTex(path, srgb);
		u32 idx = static_cast<u32>(bindlessEntries.size());
		bindlessEntries.push_back(res);
		return idx;
	};

	materials.reserve(loadedModel.materials.size());
	for (const auto& cpuMat : loadedModel.materials)
	{
		materials.emplace_back(MaterialProperties{
			.baseColor = glm::vec4(cpuMat.baseColor, cpuMat.opacity),
			.emissive = cpuMat.emissive,
			.roughness = cpuMat.roughness,
			.metallic = cpuMat.metallic,
			.ior = cpuMat.ior,
			.albedoIndex = getBindlessIndex(cpuMat.albedoPath, true),
			.normalIndex = getBindlessIndex(cpuMat.normalPath, false),
			.type = cpuMat.materialType
		});
	}

	for (u32 i = 0; i < MAX_FRAME_OVERLAP; ++i) {
		materialBuffer->UpdateBinding(i, 0, materials.data(), materials.size() * sizeof(MaterialProperties));

		DescriptorWriter writer;
		for (u32 j = 0; j < bindlessEntries.size(); ++j) {
			writer.WriteCombinedImage(1, bindlessEntries[j].first, bindlessEntries[j].second, j);
		}
		writer.UpdateSet(device, materialBuffer->descriptorSets[i]);
	}

	// Build ONE vertex buffer and ONE index buffer for the whole model
	{
		size_t totalVertices = 0;
		size_t totalIndices = 0;
		for (const auto& mesh : loadedModel.meshes)
		{
			totalVertices += mesh.unifiedVertices.size();
			totalIndices += mesh.unifiedIndices.size();
		}

		if (totalVertices == 0) return;

		const size_t vBytes = totalVertices * sizeof(Vertex);
		const size_t iBytes = totalIndices * sizeof(u32);

		// 1. Initialize GPU Buffers
		vertexBuffer.Init(device, BufferPreset::VertexStorageGPU, vBytes);
		indexBuffer.Init(device, BufferPreset::IndexGPU, iBytes);
		vertexBufferAddress = vertexBuffer.GetDeviceAddress();

		// 2. Setup Staging
		VulkanBuffer staging(device, BufferPreset::StagingUpload, vBytes + iBytes);
		auto mappedPtr = static_cast<u8*>(staging.allocationInfo.pMappedData);

		size_t vCursor = 0;
		size_t iCursor = vBytes; // Index data starts after all vertex data in staging
		u32 vGlobalOffset = 0;
		u32 iGlobalOffset = 0;

		LOG(Info, "[VulkanModel] Flattening {} meshes into unified buffers ({} verts, {} indices)",
		    loadedModel.meshes.size(), totalVertices, totalIndices);

		for (auto& mesh : loadedModel.meshes)
		{
			if (mesh.unifiedVertices.empty()) continue;

			const size_t currentVBytes = mesh.unifiedVertices.size() * sizeof(Vertex);
			const size_t currentIBytes = mesh.unifiedIndices.size() * sizeof(u32);

			std::memcpy(mappedPtr + vCursor, mesh.unifiedVertices.data(), currentVBytes);
			std::memcpy(mappedPtr + iCursor, mesh.unifiedIndices.data(), currentIBytes);

			for (const auto& part : mesh.parts)
			{
				parts.emplace_back(MeshPart{
					.aabb = part.aabb,
					.materialIndex = part.materialIndex,
					.indexCount = part.indexCount,
					.firstIndex = part.firstIndex + iGlobalOffset,
					.vertexOffset = part.vertexOffset + vGlobalOffset,
					.localTransform = part.localTransform
				});

				modelBounds.MergeAABB(part.aabb);
			}

			vCursor += currentVBytes;
			iCursor += currentIBytes;
			vGlobalOffset += static_cast<u32>(mesh.unifiedVertices.size());
			iGlobalOffset += static_cast<u32>(mesh.unifiedIndices.size());
		}

		device->immediateSubmitter.Submit([&](VkCommandBuffer cmd)
		{
			vertexBuffer.CopyFrom(cmd, &staging, vBytes, 0, 0);
			indexBuffer.CopyFrom(cmd, &staging, iBytes, vBytes, 0);
		});


		std::ranges::sort(parts, [](const MeshPart& a, const MeshPart& b)
		{
			return a.materialIndex < b.materialIndex;
		});

		LOG(Info, "[VulkanModel] Successfully uploaded geometry. Vertex BDA: 0x{:x}", vertexBufferAddress);
	}
}

void VulkanModel::Destroy()
{
	parts.clear();

	// Destroy the single, unified buffers
	if(indexBuffer.IsValid()) indexBuffer.Destroy();
	if(vertexBuffer.IsValid()) vertexBuffer.Destroy();

	materialBuffer->Destroy();
}
