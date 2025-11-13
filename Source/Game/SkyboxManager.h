#pragma once

#include "Camera.h"
#include "VulkanDescriptors.h"
#include "VulkanPipeline.h"
#include "VulkanShader.h"
#include "VulkanTexture.h"
#include "../Core/Tools/Arena.h"
#include "Tools/Array.h"

struct SkyPushConstants
{
	glm::mat4 view{};
};

using CubeSource = std::variant<
	Array<const char*, 6>, // 6 LDR images
	const char*             // 1 HDR file
>;


struct SphericalHarmonics
{
	Array<glm::vec3, 9> coefficients;
};


class SkyboxManager
{
public:
	bool Initialize(Renderer::VulkanDevice* device, ArenaAllocator* arena);

	[[nodiscard]] Renderer::VulkanImage CreateCubeMapFromSource(CubeSource source);
	Renderer::VulkanImage               CreateHDRTexture(const char* path);
	[[nodiscard]] Renderer::VulkanImage CreateCubeMapFromFiles(const Array<const char*, 6>& paths) const;

	void Render(VkCommandBuffer cmd, const Camera& camera, float aspectRatio) const;
	void Cleanup();

	// Getters for IBL integration
	[[nodiscard]] Renderer::DescriptorLayout GetLayout() const { return layout; }
	[[nodiscard]] Renderer::DescriptorSet GetDescriptorSet() const { return descriptorSet; }
	Renderer::VulkanImage& GetCubemap() { return cubemap; }

private:
	auto CreateCubemap() -> bool;
	bool CreateSampler();
	bool CreateDescriptors();
	bool CreateShaderAndPipeline();

	Renderer::VulkanDevice* devicePtr = nullptr;
	ArenaAllocator* arena = nullptr;
	SphericalHarmonics shIrradiance;

	// Skybox resources
	Renderer::VulkanImage cubemap;
	Renderer::VulkanSampler sampler;
	Renderer::VulkanShader* shader = nullptr;
	Renderer::VulkanPipeline pipeline;
	Renderer::DescriptorLayout layout;
	Renderer::DescriptorSet descriptorSet;
};

