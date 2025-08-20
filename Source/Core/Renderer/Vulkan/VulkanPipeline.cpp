//
// Created by Orgest on 6/14/2025.
//

#include "VulkanPipeline.h"

#include "Array.h"
#include "Logger.h"
#include "volk.h"

using namespace Renderer;

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

VkPipeline VulkanPipelineBuilder::BuildPipeline(VkDevice device)
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

	VkPipeline pipeline;
	VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	if (result != VK_SUCCESS)
	{
		LOG(Error, "Failed to create pipeline");
		return VK_NULL_HANDLE;
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

VulkanPipelineBuilder& VulkanPipelineBuilder::Layout(VkPipelineLayout& layout)
{
	data.config.layout = layout;
	return *this;
}
