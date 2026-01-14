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

	[[nodiscard]] Renderer::VulkanTexture CreateCubeMapFromSource(CubeSource source);
	Renderer::VulkanTexture               CreateHDRTexture(const char* path) const;
	[[nodiscard]] Renderer::VulkanTexture CreateCubeMapFromFiles(const Array<const char*, 6>& paths) const;

	void Render(Renderer::GPUCommandBuffer* cmd, const Camera& camera, f32 aspectRatio);
	void Cleanup();

	// Getters for IBL integration
	[[nodiscard]] Renderer::DescriptorLayout GetLayout() const { return layout; }
	[[nodiscard]] Renderer::DescriptorSet GetDescriptorSet() const { return descriptorSet; }
	Renderer::VulkanTexture& GetCubemap() { return cubemap; }

private:
	auto CreateCubemap() -> bool;
	void CreateSampler();
	bool CreateShaderAndPipeline();

	Renderer::VulkanDevice* devicePtr = nullptr;
	ArenaAllocator* arena = nullptr;

	// Skybox resources
	Renderer::VulkanTexture cubemap;
	Renderer::VulkanSampler sampler;
	Renderer::VulkanShader* shader = nullptr;
	Renderer::VulkanPipeline pipeline;
	Renderer::DescriptorLayout layout;
	Renderer::DescriptorSet descriptorSet;
};

