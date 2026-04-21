//
// Created by Orgest on 8/3/2025.
//

#include "VulkanMesh.h"


#include "BindlessManager.h"
#include "RendererTypes.h"
#include "TextureLoader.h"
#include "VulkanShaderBuffer.h"

#include "Tools/Timer.h"

#include "Tools/DeletionQueue.h"
#include "tracy/Tracy.hpp"


namespace Renderer
{
    Result<GPUModel> CreateVulkanModel(GPUDevice* device, LoadedModel& loadedModel, BindlessManager& bindless, AssetPool<TextureData>& texturePool, DescriptorAllocatorGrowable& allocator)
    {
        ZoneScopedN("Loading Model to GPU");

        GPUModel model;

        DescriptorSetLayoutDesc matLayoutDesc = {
            .setIndex = 1,
            .bindings = { Constants::MaterialBuffer }
        };


        model.materialBuffer = std::make_unique<VulkanShaderBuffer>(device, &allocator, matLayoutDesc);

        Vector<MaterialProperties> materialData;
        for (const auto& cpuMat : loadedModel.materials)
        {
            MaterialProperties gpuMat = {
                .baseColor = glm::vec4(cpuMat.baseColor, cpuMat.opacity),
                .emissive = cpuMat.emissive,
                .roughness = cpuMat.roughness,
                .metallic = cpuMat.metallic,
                .ior = cpuMat.ior,
                .type = cpuMat.materialType
            };


            if (cpuMat.albedoPath == "engine://white")
            {
                gpuMat.albedoIndex = BindlessManager::WhiteIdx; // Directly use Index 0
            }
            else
            {
                auto albedoH = texturePool.Load(cpuMat.albedoPath, [](const auto& p) {
                    return TextureLoader::LoadTextureFromSTB(p, true);
                });
                // Defaults to CheckerIdx (Index 2) if loading fails
                gpuMat.albedoIndex = bindless.GetOrUpload(albedoH ? *albedoH : ResourceHandle<TextureData>());
            }

            auto normalHandle = texturePool.Load(cpuMat.normalPath, [](const auto& p){
                return TextureLoader::LoadTextureFromSTB(p, false);
            });


            if (normalHandle && normalHandle->IsValid())
            {
                gpuMat.normalIndex = bindless.GetOrUpload(*normalHandle);
            }
            else
            {
                gpuMat.normalIndex = BindlessManager::NormalIdx;
            }

            materialData.push_back(gpuMat);
        }

        for (u32 i = 0; i < MAX_FRAME_OVERLAP; ++i)
        {
            model.materialBuffer->UpdateBinding(i, 0, materialData.data(), materialData.size() * sizeof(MaterialProperties));
        }


        size_t totalVertices = 0;
        size_t totalIndices = 0;
        for (const auto& m : loadedModel.meshes)
        {
            totalVertices += m.unifiedVertices.size();
            totalIndices += m.unifiedIndices.size();
        }

        if (totalVertices > 0)
        {
            const size_t vBytes = totalVertices * sizeof(Vertex);
            const size_t iBytes = totalIndices * sizeof(u32);
            const size_t totalBytes = vBytes + iBytes;


            BufferInfo vInfo = BufferInfo::FromPreset(BufferPreset::VertexStorageGPU, vBytes);
            BufferInfo iInfo = BufferInfo::FromPreset(BufferPreset::IndexGPU, iBytes);

            model.vertexBuffer = device->CreateBuffer(vInfo);
            model.indexBuffer = device->CreateBuffer(iInfo);
            model.vertexBufferAddress = model.vertexBuffer->GetDeviceAddress(); // BDA for bindless

            BufferInfo stagingInfo = BufferInfo::FromPreset(BufferPreset::StagingUpload, totalBytes);
            auto stagingBuffer = device->CreateBuffer(stagingInfo);


            Vector<u8> stagingData(totalBytes);
            u8* vPtr = stagingData.data();
            u8* iPtr = vPtr + vBytes;

            u32 vGlobalOffset = 0;
            u32 iGlobalOffset = 0;

            for (auto& mesh : loadedModel.meshes)
            {
                const size_t curVBytes = mesh.unifiedVertices.size() * sizeof(Vertex);
                const size_t curIBytes = mesh.unifiedIndices.size() * sizeof(u32);

                std::memcpy(vPtr, mesh.unifiedVertices.data(), curVBytes);
                std::memcpy(iPtr, mesh.unifiedIndices.data(), curIBytes);

                for (const auto& part : mesh.parts)
                {
                    model.parts.emplace_back(MeshPart{
                        .aabb = part.aabb,
                        .materialIndex = part.materialIndex,
                        .indexCount = part.indexCount,
                        .firstIndex = part.firstIndex + iGlobalOffset,
                        .vertexOffset = part.vertexOffset + vGlobalOffset,
                        .localTransform = part.localTransform
                    });
                    model.modelBounds.MergeAABB(part.aabb);
                }

                vPtr += curVBytes;
                iPtr += curIBytes;
                vGlobalOffset += (u32)mesh.unifiedVertices.size();
                iGlobalOffset += (u32)mesh.unifiedIndices.size();
            }

            stagingBuffer->Upload(stagingData.data(), totalBytes);

            device->ImmediateSubmit([&](GPUCommandBuffer* cmd)
            {
                cmd->CopyBuffer(stagingBuffer.get(), model.vertexBuffer.get(), vBytes, 0, 0);
                cmd->CopyBuffer(stagingBuffer.get(), model.indexBuffer.get(), iBytes, vBytes, 0);
            });
        }

        return model;
    }
}
