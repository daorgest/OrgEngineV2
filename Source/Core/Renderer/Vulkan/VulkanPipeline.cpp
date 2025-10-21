//
// Created by Orgest on 6/14/2025.
//

#include "VulkanPipeline.h"

#include "volk.h"
#include "Tools/Array.h"
#include "Tools/Logger.h"

using namespace Renderer;

void VulkanPipeline::Destroy() const
{
	if (vk != VK_NULL_HANDLE)
		vkDestroyPipeline(device->device, vk, nullptr);
	if (vkLayout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(device->device, vkLayout, nullptr);
}

static VkPipelineLayout CreatePipelineLayoutFromDesc(VkDevice device, const PipelineLayoutDesc& desc,
                                                     const std::optional<VkPushConstantRange>& builderPCR)
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

	VkPipelineLayoutCreateInfo plInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = static_cast<u32>(desc.setLayouts.size()),
		.pSetLayouts = desc.setLayouts.data(),
		.pushConstantRangeCount = pcrCount,
		.pPushConstantRanges = pPCR,
	};

	VkPipelineLayout layout = VK_NULL_HANDLE;
	vkCreatePipelineLayout(device, &plInfo, nullptr, &layout);
	return layout;
}

Result<bool> VulkanPipeline::Create(const VulkanDevice* device, const PipelineLayoutDesc& layoutDesc, const VulkanPipelineBuilder& builder)
{
	this->device = device;

	if (!device || device->device == VK_NULL_HANDLE) {
		LOG(Error, "VulkanPipeline::Create called with null VulkanDevice");
		return false;
	}
	// 0) Collect optional push-constant range from builder
	std::optional<VkPushConstantRange> maybePushConstants;
	if (builder.data.config.hasPushConstant)
	{
		maybePushConstants = builder.data.config.pushConstantRange;
	}

	// 1) Create/resolve pipeline layout
	if (builder.data.config.layout != VK_NULL_HANDLE)
	{
		vkLayout = builder.data.config.layout; // pre-supplied by caller
	}
	else
	{
		vkLayout = CreatePipelineLayoutFromDesc(device->device, layoutDesc, maybePushConstants);
		if (vkLayout == VK_NULL_HANDLE)
		{
			LOG(Error, "Failed to create VkPipelineLayout");
			return false;
		}
	}

	// 2) Determine pipeline kind based on stages
	bool hasCompute = false;
	bool hasGraphics = false;
	for (const auto& s : builder.data.shaderStages.stages)
	{
		if (s.stage & VK_SHADER_STAGE_COMPUTE_BIT) hasCompute = true;
		if (s.stage & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
			VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
			VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		{
			hasGraphics = true;
		}
	}

	// 3) Basic validation
	if (!hasCompute && !hasGraphics)
	{
		LOG(Error, "No shader stages specified for pipeline creation");
		return false;
	}
	if (hasCompute && hasGraphics)
	{
		LOG(Error, "Mixed compute+graphics stages not supported in one pipeline");
		return false;
	}

	// 4) Create pipeline
	if (hasCompute)
	{
		// Expect exactly one compute stage
		const VkPipelineShaderStageCreateInfo* stage = nullptr;
		for (const auto& s : builder.data.shaderStages.stages)
		{
			if (s.stage == VK_SHADER_STAGE_COMPUTE_BIT)
			{
				stage = &s;
				break;
			}
		}
		if (stage == nullptr)
		{
			LOG(Error, "Compute pipeline requested but no compute stage provided");
			return false;
		}

		const VkComputePipelineCreateInfo ci = {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = *stage,
			.layout = vkLayout,
		};

		if (vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &ci, nullptr, &vk) != VK_SUCCESS)
		{
			LOG(Error, "vkCreateComputePipelines failed");
			return false;
		}
	}
	else
	{
		// Graphics
		// Sanity: dynamic rendering formats
		if (builder.data.config.renderInfo.colorAttachmentCount > 0 &&
			builder.data.config.renderInfo.pColorAttachmentFormats == nullptr)
		{
			LOG(Error, "colorAttachmentCount > 0 but pColorAttachmentFormats == nullptr");
			return false;
		}

		VkPipelineViewportStateCreateInfo viewportState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1
		};

		VkPipelineColorBlendStateCreateInfo colorBlending = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &builder.data.config.colorBlendAttachment
		};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = static_cast<u32>(builder.data.config.bindings.size()),
			.pVertexBindingDescriptions = builder.data.config.bindings.data(),
			.vertexAttributeDescriptionCount = static_cast<u32>(builder.data.config.attributes.size()),
			.pVertexAttributeDescriptions = builder.data.config.attributes.data(),
		};

		VkGraphicsPipelineCreateInfo pipelineInfo = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &builder.data.config.renderInfo,
			.stageCount = static_cast<u32>(builder.data.shaderStages.stages.size()),
			.pStages = builder.data.shaderStages.stages.data(),
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &builder.data.config.inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &builder.data.config.rasterizer,
			.pMultisampleState = &builder.data.config.multisampling,
			.pDepthStencilState = &builder.data.config.depthStencil,
			.pColorBlendState = &colorBlending,
			.layout = vkLayout,
		};

		constexpr Array state = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<u32>(state.size()),
			.pDynamicStates = &state.front(),
		};
		pipelineInfo.pDynamicState = &dynamicInfo;

		if (vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vk) != VK_SUCCESS)
		{
			LOG(Error, "vkCreateGraphicsPipelines failed");
			return false;
		}
	}

	return true;
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

VulkanPipeline VulkanPipelineBuilder::BuildPipeline(VulkanDevice* device)
{
	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.viewportCount = 1,
		.scissorCount = 1
	};

	VkPipelineColorBlendStateCreateInfo colorBlending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &data.config.colorBlendAttachment
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = static_cast<u32>(data.config.bindings.size()),
		.pVertexBindingDescriptions = data.config.bindings.data(),
		.vertexAttributeDescriptionCount = static_cast<u32>(data.config.attributes.size()),
		.pVertexAttributeDescriptions = data.config.attributes.data(),
	};

	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &data.config.renderInfo,
		.stageCount = static_cast<u32>(data.shaderStages.stages.size()),
		.pStages = data.shaderStages.stages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &data.config.inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &data.config.rasterizer,
		.pMultisampleState = &data.config.multisampling,
		.pDepthStencilState = &data.config.depthStencil,
		.pColorBlendState = &colorBlending,
		.layout = data.config.layout,
	};

	Array state = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	const VkPipelineDynamicStateCreateInfo dynamicInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<u32>(state.size()),
		.pDynamicStates = &state.front(),
	};
	pipelineInfo.pDynamicState = &dynamicInfo;

	VulkanPipeline pipeline(device);
	VkResult result = vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
		&pipeline.vk);
	if (result != VK_SUCCESS)
	{
		LOG(Error, "Failed to create pipeline");
	}
	return pipeline;
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
	data.config.bindings.clear();
	for (const auto& b : bindings)
		data.config.bindings.push_back(b);

	data.config.attributes.clear();
	for (const auto& a : attributes)
		data.config.attributes.push_back(a);
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
	data.config.multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::DisableBlending()
{
	data.config.colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	data.config.colorBlendAttachment.blendEnable = VK_FALSE;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetColorAttachmentFormat(VkFormat format)
{
	data.config.colorAttachmentFormat              = format;
	data.config.renderInfo.colorAttachmentCount    = 1;
	data.config.renderInfo.pColorAttachmentFormats = &data.config.colorAttachmentFormat;
	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetDepthAttachmentFormat(VkFormat format)
{
	data.config.depthAttachmentFormat = format;
	data.config.renderInfo.depthAttachmentFormat = format;
	return* this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetDepthFormat(VkFormat format)
{
	data.config.renderInfo.depthAttachmentFormat = format;
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
