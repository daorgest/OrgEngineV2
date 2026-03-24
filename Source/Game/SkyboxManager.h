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
	bool Initialize(Renderer::VulkanDevice* device);

	[[nodiscard]] Renderer::VulkanTexture CreateCubeMapFromSource(CubeSource source);
	Renderer::VulkanTexture               CreateHDRTexture(const char* path) const;
	[[nodiscard]] Renderer::VulkanTexture CreateCubeMapFromFiles(const Array<const char*, 6>& paths) const;

	void Render(Renderer::GPUCommandBuffer* cmd, const Camera& camera, f32 aspectRatio) const;
	void Cleanup();

	// Getters for IBL integration
	[[nodiscard]] Renderer::DescriptorLayout GetLayout() const { return layout; }
	[[nodiscard]] Renderer::DescriptorSet GetDescriptorSet() const { return descriptorSet; }
	Renderer::VulkanTexture& GetCubemap() { return cubemap; }

private:
	bool CreateCubemap();
	void CreateSampler();
	bool CreateShaderAndPipeline();

	Renderer::VulkanDevice* devicePtr = nullptr;

	// Skybox resources
	Renderer::VulkanTexture cubemap;
	Renderer::VulkanSampler sampler;
	std::unique_ptr<Renderer::GPUShader> shader;
	std::unique_ptr<Renderer::GPUPipeline> pipeline;
	Renderer::DescriptorLayout layout;
	Renderer::DescriptorSet descriptorSet;
};

