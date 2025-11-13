//
// Created by Orgest on 11/4/2025.
//

#include "SceneRenderer.h"
#include "Application.h"
#include "DebugRenderer.h"
#include "SkyboxManager.h"
#include "tracy/Tracy.hpp"

void SceneRenderer::Init(const SceneRenderConfig& cfg)
{
	config = cfg;
}

void SceneRenderer::RenderModels(VkCommandBuffer cmd, u32 frameIndex, SceneStats& stats) const
{
	if (!config.models || config.models->empty())
	{
		return;
	}

	stats.drawCallCount = 0;

	// Bind scene pipeline and UBOs
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VkPipeline>(*config.scenePipeline));
	config.sceneUBO->Bind(cmd, *config.scenePipeline, frameIndex);

	// Bind skybox for IBL reflections (set 2)
	if (config.skybox)
	{
		auto [vk] = config.skybox->GetDescriptorSet();
		if (vk != VK_NULL_HANDLE)
		{
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, config.scenePipeline->vkLayout,
			                        2, 1, &vk, 0, nullptr);
		}
	}

	// Render all scene models
	Renderer::DrawCache dc{.layout = config.scenePipeline->vkLayout};
	dc.lastMat = nullptr;
	dc.lastMatSet = VK_NULL_HANDLE;
	dc.lastIndex = VK_NULL_HANDLE;

	for (const auto& inst : *config.models)
	{
		const Renderer::VulkanModel* mdl = inst.model;
		if (!mdl || mdl->parts.empty() || !mdl->indexBuffer.IsValid() || !mdl->vertexBuffer.IsValid())
		{
			continue;
		}

		const glm::mat4 instM = inst.transform;

		// Bind index buffer once per model
		VkBuffer ibuf = mdl->indexBuffer.buffer;
		if (ibuf != dc.lastIndex)
		{
			vkCmdBindIndexBuffer(cmd, ibuf, 0, VK_INDEX_TYPE_UINT32);
			dc.lastIndex = ibuf;
		}

		const uint64_t vertexBufferAddress = mdl->vertexAddress;

		for (const auto& part : mdl->parts)
		{
			if (part.indexCount == 0) continue;
			if (part.materialIndex >= mdl->materials.size()) continue;
			if (stats.drawCallCount >= config.drawLimit) break;

			const Renderer::VulkanMaterial& mat = mdl->materials[part.materialIndex];
			VkDescriptorSet matSet = mat.descriptorSet.vk;

			// Bind material descriptor set
			if (matSet != dc.lastMatSet)
			{
				dc.lastMatSet = matSet;
				dc.lastMat = &mat;
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dc.layout,
				                        1, 1, &matSet, 0, nullptr);
			}

			// Calculate world matrix
			const glm::mat4 worldMatrix = (part.transform == glm::mat4(1.0f)) ? instM : (instM * part.transform);

			// Set push constants - use material PBR properties multiplied by instance overrides
			PushConstants pc = {
				.worldMatrix = worldMatrix,
				.normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldMatrix))),
				.vertexOffset = part.vertexOffset,
				.deviceAddress = vertexBufferAddress,
				.roughness = mat.roughness * inst.roughness,
				.metallic = mat.metallic * inst.metallic,
				.baseColorTint = mat.baseColor
			};

			vkCmdPushConstants(cmd, dc.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			                   0, sizeof(PushConstants), &pc);

			vkCmdDrawIndexed(cmd, part.indexCount, 1, part.firstIndex, 0, 0);

			if (config.debugRenderer && config.debugRenderer->enabled)
			{
				config.debugRenderer->QueueBox(pc.worldMatrix, part.localBounds.Min(), part.localBounds.Max());
			}

			++stats.drawCallCount;
		}
	}
}
