//
// Created by Orgest on 6/14/2025.
//

#include "VulkanPipeline.h"

#include "volk.h"
#include "VulkanConvert.h"
#include "VulkanShader.h"
#include "Tools/Array.h"
#include "Tools/Logger.h"

using namespace Renderer;

void VulkanPipeline::Destroy()
{
	if (device && device->device != VK_NULL_HANDLE)
	{
		if (vk != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device->device, vk, nullptr);
			vk = VK_NULL_HANDLE;
		}
		if (vkLayout != VK_NULL_HANDLE && ownsLayout)
		{
			vkDestroyPipelineLayout(device->device, vkLayout, nullptr);
			vkLayout = VK_NULL_HANDLE;
		}
	}
	ownsLayout = false;
}

static VkPipelineLayout CreatePipelineLayoutFromDesc(VkDevice device, const PipelineLayoutDesc& desc, const std::optional<VkPushConstantRange>& builderPCR)
{
	const VkPushConstantRange* pPCR = nullptr;
	u32 pcrCount = 0;

	if (!desc.pushRanges.empty())
	{
		pPCR = desc.pushRanges.data();
		pcrCount = static_cast<u32>(desc.pushRanges.size());
	}
	else if (builderPCR.has_value() && builderPCR->size > 0)
	{
		pPCR = &(*builderPCR);
		pcrCount = 1;
	}

	VkPipelineLayoutCreateInfo plInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = static_cast<u32>(desc.setLayouts.size()),
		.pSetLayouts = desc.setLayouts.data(),
		.pushConstantRangeCount = pcrCount,
		.pPushConstantRanges = pPCR,
	};

	VkPipelineLayout layout = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(device, &plInfo, nullptr, &layout));
	return layout;
}

void VulkanPipeline::Rebuild(GPUDevice* device)
{
	LOG(Error, "VulkanPipeline::Rebuild() not supported (no hot reload state stored).");
}

Result<bool> VulkanPipeline::Create(VulkanDevice* dev, const PipelineLayoutDesc& layoutDesc, const VulkanPipelineBuilder& builder)
{
    device = dev;

    if (!device || device->device == VK_NULL_HANDLE)
    {
        LOG(Error, "VulkanPipeline::Create: null device");
        return false;
    }

    std::optional<VkPushConstantRange> maybePCR;
    if (builder.data.config.hasPushConstant)
        maybePCR = builder.data.config.pushConstantRange;

    if (builder.data.config.layout != VK_NULL_HANDLE)
    {
        vkLayout = builder.data.config.layout;
        ownsLayout = false;
    }
    else
    {
        vkLayout = CreatePipelineLayoutFromDesc(device->device, layoutDesc, maybePCR);
        ownsLayout = true;
    }

    bool hasCompute  = false;
    bool hasGraphics = false;

    for (const auto& s : builder.data.shaderStages.stages)
    {
        if (s.stage == VK_SHADER_STAGE_COMPUTE_BIT)
            hasCompute = true;

        if (s.stage & (VK_SHADER_STAGE_VERTEX_BIT |
                       VK_SHADER_STAGE_FRAGMENT_BIT |
                       VK_SHADER_STAGE_GEOMETRY_BIT |
                       VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                       VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
            hasGraphics = true;
    }

    if (!hasCompute && !hasGraphics)
    {
        LOG(Error, "Pipeline has no shader stages");
        return false;
    }

    if (hasCompute && hasGraphics)
    {
        LOG(Error, "Cannot mix compute and graphics stages");
        return false;
    }

    if (hasCompute)
    {
        const VkPipelineShaderStageCreateInfo* stage = nullptr;

        for (const auto& s : builder.data.shaderStages.stages)
        {
            if (s.stage == VK_SHADER_STAGE_COMPUTE_BIT)
            {
                stage = &s;
                break;
            }
        }

        if (!stage)
        {
            LOG(Error, "Compute pipeline requested but no compute stage provided");
            return false;
        }

        VkComputePipelineCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = *stage,
            .layout = vkLayout,
        };

        VK_CHECK(vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &ci, nullptr, &vk));
        return true;
    }

    // GRAPHICS
    const auto& cfg = builder.data.config;

    // if (cfg.colorAttachments > 0 && cfg.renderInfo.pColorAttachmentFormats == nullptr)
    // {
    //     LOG(Error, "colorAttachmentCount > 0 but pColorAttachmentFormats is null");
    //     return false;
    // }

    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &cfg.colorBlendAttachment
    };

    VkPipelineVertexInputStateCreateInfo vertex = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = (u32)cfg.bindings.size(),
        .pVertexBindingDescriptions = cfg.bindings.data(),
        .vertexAttributeDescriptionCount = (u32)cfg.attributes.size(),
        .pVertexAttributeDescriptions = cfg.attributes.data(),
    };

    static constexpr VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynStates
    };

    VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &cfg.renderInfo,
        .stageCount = (u32)builder.data.shaderStages.stages.size(),
        .pStages = builder.data.shaderStages.stages.data(),
        .pVertexInputState = &vertex,
        .pInputAssemblyState = &cfg.inputAssembly,
        .pViewportState = &viewport,
        .pRasterizationState = &cfg.rasterizer,
        .pMultisampleState = &cfg.multisampling,
        .pDepthStencilState = &cfg.depthStencil,
        .pColorBlendState = &blend,
        .pDynamicState = &dynInfo,
        .layout = vkLayout,
    };

    VK_CHECK(vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &info, nullptr, &vk));
    return true;
}

Result<VkPipeline> VulkanPipelineBuilder::BuildGraphicsPipeline(const VulkanDevice* device, const VulkanPipelineBuilder& b, VkPipelineLayout layout)
{
	const auto& cfg = b.data.config;
	const auto& stages = b.data.shaderStages.stages;
	auto vertexInputInfo  = MakeVertexInputInfo(b.data.config);
	auto viewportInfo     = MakeViewportInfo();
	auto blendInfo        = MakeBlendInfo(cfg.colorBlendAttachment);
	auto dynamicStateInfo = MakeDynamicStateInfo();

	VkGraphicsPipelineCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &cfg.renderInfo,
		.stageCount = static_cast<u32>(stages.size()),
		.pStages = stages.data(),
		.pVertexInputState   = &vertexInputInfo,
		.pInputAssemblyState = &cfg.inputAssembly,
		.pViewportState      = &viewportInfo,
		.pRasterizationState = &cfg.rasterizer,
		.pMultisampleState   = &cfg.multisampling,
		.pDepthStencilState  = &cfg.depthStencil,
		.pColorBlendState    = &blendInfo,
		.pDynamicState       = &dynamicStateInfo,
		.layout              = layout,
	};

	VkPipeline pipeline{};
	if (vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
		return std::unexpected(VulkanPipelineCreationFailed);

	LogPipelineStages(stages);
	return pipeline;
}

VulkanPipelineBuilder::VulkanPipelineBuilder()
{
	data.config.inputAssembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
	};

	data.config.rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
	};

	data.config.colorBlendAttachment = {};

	data.config.multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
	};

	data.config.layout = VK_NULL_HANDLE;

	data.config.depthStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
	};

	data.config.renderInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO
	};
	data.shaderStages.stages.clear();
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetComputeShader(VkShaderModule computeShader)
{
	VkPipelineShaderStageCreateInfo computeMain = {
		.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage  = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = computeShader,
		.pName  = "computeMain"
	};

	data.shaderStages.stages.push_back(computeMain);
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetFragVerShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
	// data.shaderStages.stages.clear();

	VkPipelineShaderStageCreateInfo vertStage = {
		.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage  = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vertexShader,
		.pName  = "vertexMain"
	};

	VkPipelineShaderStageCreateInfo fragStage = {
		.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = fragmentShader,
		.pName  = "fragmentMain"
	};

	data.shaderStages.stages.push_back(vertStage);
	data.shaderStages.stages.push_back(fragStage);

	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetInputTopology(VkPrimitiveTopology topology)
{
	data.config.inputAssembly.topology               = topology;
	data.config.inputAssembly.primitiveRestartEnable = VK_FALSE;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetVertexInput(std::span<const VkVertexInputBindingDescription> bindings,
	std::span<const VkVertexInputAttributeDescription> attributes)
{
	data.config.bindings.assign(bindings.begin(), bindings.end());
	data.config.attributes.assign(attributes.begin(), attributes.end());

	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetVertexInput(const VertexInputLayout& layout)
{

	data.config.bindings.clear();
	data.config.attributes.clear();

	data.config.bindings.reserve(layout.bindings.size());
	data.config.attributes.reserve(layout.attributes.size());

	for (const auto& b : layout.bindings)
	{
		data.config.bindings.push_back(ToVk(b));
	}

	for (const auto& a : layout.attributes)
	{
		data.config.attributes.push_back(ToVk(a));
	}

	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetPushConstantRange(u32 size, VkShaderStageFlags stageFlags, u32 offset)
{
	data.config.pushConstantRange.stageFlags = stageFlags;
	data.config.pushConstantRange.offset = offset;
	data.config.pushConstantRange.size = size;
	data.config.hasPushConstant = true;
	return *this;
}

// Raster states
VulkanPipelineBuilder& VulkanPipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
	data.config.rasterizer.polygonMode = mode;
	data.config.rasterizer.lineWidth   = 1.0f;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	data.config.rasterizer.cullMode  = cullMode;
	data.config.rasterizer.frontFace = frontFace;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::EnableMultisampling(VkSampleCountFlagBits sampleCount)
{
	data.config.multisampling.rasterizationSamples = sampleCount;
	return *this;
}

// multisample state
VulkanPipelineBuilder& VulkanPipelineBuilder::SetMultisamplingNone()
{
	data.config.multisampling = {};
	data.config.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	data.config.multisampling.flags = 0;
	data.config.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	data.config.multisampling.sampleShadingEnable = VK_FALSE;
	data.config.multisampling.minSampleShading = 1.0f;
	data.config.multisampling.pSampleMask = nullptr;
	data.config.multisampling.alphaToCoverageEnable = VK_FALSE;
	data.config.multisampling.alphaToOneEnable = VK_FALSE;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::DisableBlending()
{
	data.config.colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	data.config.colorBlendAttachment.blendEnable = VK_FALSE;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetColorAttachmentFormat(TextureFormat format)
{
	data.config.colorAttachmentFormat              = ToVkFormat(format);
	data.config.renderInfo.colorAttachmentCount    = 1;
	data.config.renderInfo.pColorAttachmentFormats = &data.config.colorAttachmentFormat;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetDepthAttachmentFormat(TextureFormat format)
{
	const VkFormat vkFormat = ToVkFormat(format);
	data.config.depthAttachmentFormat = vkFormat;
	data.config.renderInfo.depthAttachmentFormat = vkFormat;
	return* this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetDepthFormat(TextureFormat format)
{
	data.config.renderInfo.depthAttachmentFormat = ToVkFormat(format);
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::DisableDepthTest()
{
	data.config.depthStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_FALSE,
		.depthWriteEnable = VK_FALSE,
		.depthCompareOp = VK_COMPARE_OP_NEVER,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::EnableDepthTest(bool depthWriteEnable, VkCompareOp op)
{
	data.config.depthStencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = depthWriteEnable,
		.depthCompareOp = op,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::EnableBlendingAdditive()
{
	data.config.colorBlendAttachment = {
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
		                  VK_COLOR_COMPONENT_A_BIT
	};
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::EnableBlendingAlphaBlend()
{
	data.config.colorBlendAttachment = {
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
		                  VK_COLOR_COMPONENT_A_BIT
	};
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::Layout(const VkPipelineLayout& layout)
{
	data.config.layout = layout;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::EnableHotReload(const char* shaderSourcePath, bool isCompute)
{
	data.config.enableHotReload = true;
	data.config.shaderSourcePath = shaderSourcePath;
	data.config.isComputeShader = isCompute;
	return *this;
}

VkPipelineDynamicStateCreateInfo VulkanPipelineBuilder::MakeDynamicStateInfo()
{
	static constexpr VkDynamicState states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	return {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<u32>(std::size(states)),
		.pDynamicStates = states
	};
}

VkPipelineViewportStateCreateInfo VulkanPipelineBuilder::MakeViewportInfo()
{
	return {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};
}

VkPipelineColorBlendStateCreateInfo VulkanPipelineBuilder::MakeBlendInfo(const VkPipelineColorBlendAttachmentState& src)
{
	return {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &src
	};
}

VkPipelineVertexInputStateCreateInfo VulkanPipelineBuilder::MakeVertexInputInfo(const PipelineConfig& cfg)
{
	return {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = static_cast<u32>(cfg.bindings.size()),
		.pVertexBindingDescriptions = cfg.bindings.data(),
		.vertexAttributeDescriptionCount = static_cast<u32>(cfg.attributes.size()),
		.pVertexAttributeDescriptions = cfg.attributes.data(),
	};
}

void VulkanPipelineBuilder::LogPipelineStages(const Vector<VkPipelineShaderStageCreateInfo>& stages)
{
	for (u32 i = 0; i < stages.size(); ++i)
	{
		const auto& stage = stages[i];
		const char* name =
			(stage.stage == VK_SHADER_STAGE_VERTEX_BIT)   ? "VERTEX" :
			(stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT) ? "FRAGMENT" :
			(stage.stage == VK_SHADER_STAGE_COMPUTE_BIT)  ? "COMPUTE" : "OTHER";

		LOG(Info, "  Stage {}: {} (entry: {})", i, name, stage.pName);
	}
}