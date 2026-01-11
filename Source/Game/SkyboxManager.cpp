#include "SkyboxManager.h"

#include "../Engine/ShaderConstants.h"
#include "../Engine/TextureLoader.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanTexture.h"
#include "glm/gtc/constants.hpp"
#include "Tools/Array.h"
#include "Tools/Logger.h"

using namespace Renderer;

VulkanTexture SkyboxManager::CreateCubeMapFromSource(CubeSource source)
{
	return std::visit([this]<typename T0>(T0&& arg) -> VulkanTexture
	{
		using T = std::decay_t<T0>;
		if constexpr (std::is_same_v<T, Array<const char*, 6>>)
		{
			// existing 6-image code
			LOG(Debug, "Detected 6 LDR images for cubemap");
			return CreateCubeMapFromFiles(arg);
		}
		else if constexpr (std::is_same_v<T, const char*>)
		{
			// single HDR image
			LOG(Debug, "Detected single HDR image for cubemap");

			return CreateHDRTexture(arg);
		}
		return {}; // NOTHINGGGGGGG
	}, source);
}

VulkanTexture SkyboxManager::CreateHDRTexture(const char* path) const
{
	const auto tex = TextureLoader::LoadHDRTextureFromSTB(path);
	if (!tex)
	{
		LOG(Error, "Failed to load HDR cubemap: {}", path);
		return {};
	}
	const auto& pixels = std::get<Vector<f32>>(tex->data);
	assert(std::holds_alternative<Vector<f32>>(tex->data) && "Expected HDR f32 texture data");


	TextureInfo info = {
		.extent = {static_cast<u32>(tex->width), static_cast<u32>(tex->height), 1},
		.type = ImageType::CubeMap,
		.format = TextureFormat::RGBA32_SFLOAT,
		.dimension = TextureDimension::CubeMap,
		.usage = ImageUsage::Sampled | ImageUsage::TransferDst
	};
	VulkanTexture cube(devicePtr, info);

	cube.UploadTextureToGPU(pixels.data(), info);

	return cube;
}

VulkanTexture SkyboxManager::CreateCubeMapFromFiles(const Array<const char*, 6>& paths) const
{
	LOG(Debug, "CreateCubeMap: Loading 6 cubemap faces...");

	Array<TextureData, 6> faces;
	for (int i = 0; i < 6; ++i)
	{
		auto tex = TextureLoader::LoadTextureFromSTB(paths[i], true);
		if (!tex.has_value())
		{
			LOG(Warning, "CreateCubeMap: Failed to load face {}", i);
			return {};
		}

		if (!std::holds_alternative<Vector<u8>>(tex->data))
		{
			LOG(Warning, "CreateCubeMap: Face {} has invalid data type", i);
			return {};
		}

		faces[i] = std::move(*tex);
	}

	// Validate base face
	const u32 w = faces[0].width;
	const u32 h = faces[0].height;
	if (w == 0 || h == 0)
	{
		LOG(Warning, "CreateCubeMap: Invalid dimensions on face 0 ({}x{})", w, h);
		return {};
	}

	const size_t faceBytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;

	// Validate all faces
	for (size_t i = 1; i < 6; ++i)
	{
		if (faces[i].width != w || faces[i].height != h)
		{
			LOG(Warning, "CreateCubeMap: Face {} has mismatched dimensions ({}x{} vs 0={}x{})",
				i, faces[i].width, faces[i].height, w, h);
			return {};
		}

		// we REALLY DON'T WANT ANOTHER TYPE TO GO THROUGH
		if (!std::holds_alternative<Vector<u8>>(faces[i].data) || std::get<Vector<u8>>(faces[i].data).size() < faceBytes)
		{
			LOG(Warning, "CreateCubeMap: Face {} has insufficient or invalid data", i);
			return {};
		}
	}

	TextureInfo info{
		.extent = { w, h, 1 },
		.mipLevels = 1,
		.arrayLayers = 6,
		.type = ImageType::CubeMap,
		.format = TextureFormat::RGBA8_SRGB,
		.dimension = TextureDimension::CubeMap,
		.usage = ImageUsage::Sampled | ImageUsage::TransferDst,
	};

	VulkanTexture cube(devicePtr, info);

	// Pack all faces into a single contiguous buffer
	Vector<u8> packed;
	packed.resize(faceBytes * 6);
	for (u32 i = 0; i < 6; ++i)
	{
		const auto& faceData = std::get<Vector<u8>>(faces[i].data);
		std::memcpy(packed.data() + (static_cast<size_t>(i) * faceBytes), faceData.data(), faceBytes);
	}

	cube.UploadTextureToGPU(packed.data(), info);

	LOG(Debug, "CreateCubeMap: Successfully created cubemap ({}x{}, 6 faces)", w, h);
	return cube;
}

bool SkyboxManager::Initialize(VulkanDevice* dev, ArenaAllocator* arenaAlloc)
{
	devicePtr = dev;
	arena = arenaAlloc;

	if (!CreateCubemap()) return false;
	CreateSampler();
	if (!CreateShaderAndPipeline()) return false;

	return true;
}

bool SkyboxManager::CreateCubemap()
{
	constexpr Array skyFaces = {
		"Assets/Skybox/right.jpg",
		"Assets/Skybox/left.jpg",
		"Assets/Skybox/top.jpg",
		"Assets/Skybox/bottom.jpg",
		"Assets/Skybox/front.jpg",
		"Assets/Skybox/back.jpg"
	};

	cubemap = CreateCubeMapFromFiles(skyFaces);

	// Fallback: procedural gradient if files don't exist
	if (cubemap.image == nullptr)
	{
		LOG(Warning, "Skybox: Texture files not found, creating procedural gradient...");

		TextureInfo cubeInfo{
			.extent = {1, 1, 1},
			.mipLevels = 1,
			.arrayLayers = 6,
			.type = ImageType::CubeMap,
			.format = TextureFormat::RGBA8_UNORM,
			.dimension = TextureDimension::CubeMap,
			.usage = ImageUsage::Sampled | ImageUsage::TransferDst,
		};

		cubemap = VulkanTexture(devicePtr, cubeInfo);

		Array<u32, 6> faceColors = {
			0xFFFFAA88, // +X right
			0xFFFFAA88, // -X left
			0xFFFFDD99, // +Y top
			0xFF666688, // -Y bottom
			0xFFFFAA88, // +Z front
			0xFFFFAA88  // -Z back
		};

		Vector<u8> faceData(6 * 4);
		for (size_t i = 0; i < 6; ++i)
		{
			std::memcpy(faceData.data() + (i * 4), &faceColors[i], 4);
		}

		cubemap.UploadTextureToGPU(faceData.data(), cubeInfo);
	}
	else
	{
		LOG(Info, "Skybox: Successfully loaded cubemap from files");
	}

	return true;
}

void SkyboxManager::CreateSampler()
{
	SamplerInfo sampDesc = {
		.minFilter = SamplerFilter::Linear,
		.magFilter = SamplerFilter::Linear,
		.mipFilter = SamplerMipFilter::Linear,
	};
	sampler = VulkanSampler(devicePtr, sampDesc);
}

bool SkyboxManager::CreateShaderAndPipeline()
{
	auto codeResult = VulkanShader::ReadShaderFile("Shaders/skybox.spv");
	if (!codeResult)
		LOG(Error, "Skybox: Could not load shader (error {})", static_cast<int>(codeResult.error()));

	shader = arena->Emplace<VulkanShader>(devicePtr, codeResult.value());

	// Descriptors
	{
		DescriptorLayoutBuilder b;
		layout = b.AddBindings(Constants::Skybox)
		          .Build(devicePtr);
	}

	Array<PoolSizes, 1> poolSizes = {
		{DescriptorType::CombinedImageSampler, 1.0f},
	};

	static DescriptorAllocatorGrowable skyPool;
	skyPool.Init(devicePtr, 1, poolSizes);
	descriptorSet = skyPool.Allocate(layout);

	DescriptorWriter()
		.WriteCombinedImage(0, &cubemap, &sampler, 0)
		.UpdateSet(devicePtr, descriptorSet);

	static DescriptorSetLayoutDesc setLayout = {
		.setIndex = 2,
		.bindings = Constants::Skybox,
	};

	static PushConstantDesc pushConstant = {
		.size   = sizeof(SkyPushConstants),
		.offset = 0,
		.stages = ShaderStage::Vertex
	};

	const GraphicsPipelineDesc skyboxDesc = {
		.vertexShader   = shader,
		.fragmentShader = shader,
		.raster = {
			.topology    = PrimitiveTopology::TriangleList,
			.cull        = CullMode::None,
			.depthFormat = TextureFormat::D32_SFLOAT,
			.depthWrite  = false,
			.depthOp     = CompareOp::GreaterOrEqual,
			.blend       = { .enabled = false },
			.colorFormats = { TextureFormat::BGRA8_SRGB }
		},
		.layout = {
			.setLayouts   = SPAN_ONE(setLayout),
			.pushConstants = SPAN_ONE(pushConstant)
		}
	};

	const auto result = pipeline.CreateGraphicsPipeline(devicePtr, skyboxDesc);
	return result.has_value();

	LOG(Info, "Skybox: Successfully loaded pipeline");
}

void SkyboxManager::Render(GPUCommandBuffer* cmd, const Camera& camera, f32 aspectRatio)
{
	if (!shader || !pipeline.IsValid() || cubemap.image == nullptr)
		return;

	cmd->BindPipeline(&pipeline);
	cmd->BindDescriptorSet(&descriptorSet, 0, &pipeline);

	const SkyPushConstants skyPC = {
		.view = camera.GetProjectionMatrix(aspectRatio) * glm::mat4(glm::mat3(camera.GetViewMatrix(glm::vec3{0.0f})))
	};

	cmd->PushConstants(&pipeline, ShaderStage::Vertex, 0, sizeof(SkyPushConstants), &skyPC);
	cmd->Draw(36, 1, 0, 0);
}

void SkyboxManager::Cleanup()
{
	if (pipeline.IsValid()) pipeline.Destroy();
	if (shader) { shader->Destroy(); }
	if (cubemap.image) cubemap.Destroy();
	sampler.Destroy();
	layout.Destroy(devicePtr);
}

