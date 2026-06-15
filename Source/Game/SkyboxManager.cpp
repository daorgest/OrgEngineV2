#include "SkyboxManager.h"

#include "../Engine/ShaderConstants.h"
#include "../Engine/TextureLoader.h"
#include "Tools/Array.h"
#include "Tools/Logger.h"

using namespace Renderer;

std::unique_ptr<GPUTexture> SkyboxManager::CreateCubeMapFromSource(CubeSource source)
{
	return std::visit([this]<typename T0>(T0&& arg) -> std::unique_ptr<GPUTexture>
	{
		using T = std::decay_t<T0>;
		if constexpr (std::is_same_v<T, Array<const char*, 6>>)
		{

			LOG(Debug, "Detected 6 LDR images for cubemap");
			return CreateCubeMapFromFiles(arg);
		}
		else if constexpr (std::is_same_v<T, const char*>)
		{
			LOG(Debug, "Detected single HDR image for cubemap");

			return CreateHDRTexture(arg);
		}
		return {}; // NOTHINGGGGGGG
	}, source);
}

std::unique_ptr<GPUTexture> SkyboxManager::CreateHDRTexture(const char* path) const
{
    const auto tex = TextureLoader::LoadHDRTextureFromSTB(path);
    if (!tex) return nullptr;


    TextureInfo sourceInfo = {
        .extent = {static_cast<u32>(tex->width), static_cast<u32>(tex->height), 1},
        .format = TextureFormat::RGBA32_SFLOAT,
        .dimension = TextureDimension::Texture2D,
        .usage = ImageUsage::Sampled | ImageUsage::TransferDst
    };
    const auto hdr2D = devicePtr->CreateTexture(sourceInfo);
    hdr2D->UploadData(std::get<Vector<f32>>(tex->data).data());

    TextureInfo cubeInfo = {
        .extent = {512, 512, 1},
        .arrayLayers = 6,
        .type = ImageType::CubeMap,
        .format = TextureFormat::RGBA32_SFLOAT,
        .dimension = TextureDimension::CubeMap,
        .usage = ImageUsage::Sampled | ImageUsage::ColorAttachment | ImageUsage::TransferSrc
    };
    auto finalCubemap = devicePtr->CreateTexture(cubeInfo);


    // BakeHDRToCubemap(hdr2D.get(), finalCubemap.get());

    return finalCubemap;
}



std::unique_ptr<GPUTexture> SkyboxManager::CreateCubeMapFromFiles(const Array<const char*, 6>& paths) const
{
	Array<TextureData, 6> faces;
	for (i32 i = 0; i < 6; ++i)
	{
	    auto tex = TextureLoader::LoadTextureFromSTB(paths[i], true);
	    if (!tex || !std::holds_alternative<Vector<u8>>(tex->data)) return {};
	    faces[i] = std::move(*tex);
	}

    const u32 w = faces[0].width;
    const u32 h = faces[0].height;
    const size_t faceBytes = static_cast<size_t>(w) * h * 4;

	// // Validate all faces
	// for (size_t i = 1; i < 6; ++i)
	// {
	// 	if (faces[i].width != w || faces[i].height != h)
	// 	{
	// 		LOG(Warning, "CreateCubeMap: Face {} has mismatched dimensions ({}x{} vs 0={}x{})",
	// 			i, faces[i].width, faces[i].height, w, h);
	// 		return {};
	// 	}
	//
	// 	// we REALLY DON'T WANT ANOTHER TYPE TO GO THROUGH
	// 	if (!std::holds_alternative<Vector<u8>>(faces[i].data) || std::get<Vector<u8>>(faces[i].data).size() < faceBytes)
	// 	{
	// 		LOG(Warning, "CreateCubeMap: Face {} has insufficient or invalid data", i);
	// 		return {};
	// 	}
	// }

	TextureInfo info = {
		.extent = { w, h, 1 },
		.mipLevels = 1,
		.arrayLayers = 6,
		.type = ImageType::CubeMap,
		.format = TextureFormat::RGBA8_SRGB,
		.dimension = TextureDimension::CubeMap,
		.usage = ImageUsage::Sampled | ImageUsage::TransferDst,
	};

	Vector<u8> packed(faceBytes * 6);
	for (u32 i = 0; i < 6; ++i)
	{
		const auto& faceData = std::get<Vector<u8>>(faces[i].data);
		std::memcpy(packed.data() + (static_cast<size_t>(i) * faceBytes), faceData.data(), faceBytes);
	}

    auto cube = devicePtr->CreateTexture(info);
    if (cube) {
        cube->UploadData(packed.data());
        cube->SetName("Skybox Cubemap");
    }

	LOG(Debug, "CreateCubeMap: Successfully created cubemap ({}x{}, 6 faces)", w, h);
	return cube;
}

bool SkyboxManager::Initialize(GPUDevice* device, DescriptorAllocatorGrowable& descriptorAlloc)
{
    devicePtr = device;

    constexpr Array skyFaces = {
        "Assets/Skybox/right.jpg",
        "Assets/Skybox/left.jpg",
        "Assets/Skybox/top.jpg",
        "Assets/Skybox/bottom.jpg",
        "Assets/Skybox/front.jpg",
        "Assets/Skybox/back.jpg"
    };

    cubemap = CreateCubeMapFromFiles(skyFaces);
    if (!cubemap)
    {
        cubemap = CreateProceduralFallback();
    }

    SamplerInfo sampDesc = {
        .minFilter = SamplerFilter::Linear,
        .magFilter = SamplerFilter::Linear
    };
    sampler = device->CreateSampler(sampDesc);

    return CreateShaderAndPipeline(descriptorAlloc);
}

std::unique_ptr<GPUTexture> SkyboxManager::CreateProceduralFallback() const
{
    TextureInfo info = {
        .extent = {1, 1, 1},
        .arrayLayers = 6,
        .type = ImageType::CubeMap,
        .format = TextureFormat::RGBA8_UNORM,
        .usage = ImageUsage::Sampled | ImageUsage::TransferDst
    };

    auto cube = devicePtr->CreateTexture(info);
    constexpr u32 colors[6] = { 0xFFFFAA88, 0xFFFFAA88, 0xFFFFDD99, 0xFF666688, 0xFFFFAA88, 0xFFFFAA88 };
    cube->UploadData(colors);
    return cube;
}

bool SkyboxManager::CreateShaderAndPipeline(DescriptorAllocatorGrowable& descriptorAlloc)
{
    shader = devicePtr->CreateShaderPath("Shaders/skybox.spv");

	// Descriptors
	{
		DescriptorLayoutBuilder b;
		layout = b.AddBindings(Constants::Skybox)
		          .Build(devicePtr);
	}

    descriptorSet = descriptorAlloc.Allocate(layout);

    DescriptorWriter()
        .WriteImage(0, cubemap->GetView(), sampler.get(), DescriptorType::CombinedImageSampler)
        .UpdateSet(devicePtr, descriptorSet);

    PipelineLayoutDesc skyLayout;
    skyLayout.setLayouts = {DescriptorSetLayoutDesc::FromConstants(0, Constants::Skybox)};

    skyLayout.pushConstants = {
        {
            .size = sizeof(SkyPushConstants),
            .offset = 0,
            .stages = ShaderStage::Vertex
        }
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
            .sampleCount = devicePtr->currentSamples,
            .colorFormats = {TextureFormat::BGRA8_SRGB},
        },
		.layout = skyLayout
	};

    pipeline = devicePtr->CreateGraphicsPipeline(skyboxDesc);
	LOG(Info, "Skybox: Successfully loaded pipeline");
    return pipeline.get();
}

void SkyboxManager::Render(GPUCommandBuffer* cmd, const Camera& camera) const
{
    if (!pipeline || !cubemap)
        return;

    cmd->BindPipeline(pipeline.get());
    cmd->BindDescriptorSet(&descriptorSet, 0, pipeline.get());

	const SkyPushConstants skyPC = {
		.view = camera.projection * glm::mat4(glm::mat3(camera.view))
	};

	cmd->PushConstants(pipeline.get(), ShaderStage::Vertex, 0, sizeof(SkyPushConstants), &skyPC);
	cmd->Draw(36, 1, 0, 0);
}

void SkyboxManager::Cleanup() const
{
    auto* nonConstThis = const_cast<SkyboxManager*>(this);

    nonConstThis->pipeline.reset();
    nonConstThis->cubemap.reset();
    nonConstThis->sampler.reset();
    nonConstThis->shader.reset();

    layout.Destroy(devicePtr);
}

