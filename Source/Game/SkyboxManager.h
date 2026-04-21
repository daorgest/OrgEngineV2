#pragma once

#include "Camera.h"
#include "RenderInterface.h"
#include "VulkanDescriptors.h"
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
    bool Initialize(Renderer::GPUDevice* device);


    [[nodiscard]] std::unique_ptr<Renderer::GPUTexture> CreateCubeMapFromSource(CubeSource source);
    std::unique_ptr<Renderer::GPUTexture>               CreateHDRTexture(const char* path) const;
    std::unique_ptr<Renderer::GPUTexture> CreateCubeMapFromFiles(const Array<const char*, 6>& paths) const;

    void Render(Renderer::GPUCommandBuffer* cmd, const Camera& camera) const;
    void Cleanup() const;


   [[nodiscard]] Renderer::GPUTexture* GetCubemap() const { return cubemap.get(); }
   [[nodiscard]] Renderer::DescriptorSet GetDescriptorSet() const { return descriptorSet; }

private:
    std::unique_ptr<Renderer::GPUTexture> CreateProceduralFallback() const;
    bool CreateShaderAndPipeline();

    Renderer::GPUDevice* devicePtr = nullptr;

    // Skybox resources
    std::unique_ptr<Renderer::GPUTexture> cubemap;
    std::unique_ptr<Renderer::GPUSampler> sampler;
    std::shared_ptr<Renderer::GPUShader> shader;
    std::unique_ptr<Renderer::GPUPipeline> pipeline;
    Renderer::DescriptorLayout layout;
    Renderer::DescriptorSet descriptorSet;
};

