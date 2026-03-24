//
// Created by Orgest on 6/14/2025.
//

#include "VulkanPipeline.h"

#include <volk.h>
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "Tools/Logger.h"

using namespace Renderer;

VulkanPipelineBuilder::VulkanPipelineBuilder()
{
	data.config.inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	data.config.rasterizer    = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	data.config.multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	data.config.depthStencil  = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	data.config.renderInfo    = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

	data.shaderStages.stages.clear();
}

VulkanPipelineBuilder& VulkanPipelineBuilder::ApplyRasterDesc(const GpuRasterDesc& raster)
{
	data.config.inputAssembly.topology = ToVk(raster.topology);

	data.config.rasterizer.cullMode  = ToVk(raster.cull);
	data.config.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	data.config.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	data.config.rasterizer.lineWidth   = 1.0f;

	data.config.multisampling.rasterizationSamples = ToVk(raster.sampleCount);
	data.config.multisampling.alphaToCoverageEnable = raster.alphaToCoverage ? VK_TRUE : VK_FALSE;

	if (raster.depthFormat != TextureFormat::UNKNOWN)
	{
		data.config.depthAttachmentFormat = ToVkFormat(raster.depthFormat);
		data.config.depthStencil.depthTestEnable   = VK_TRUE;
		data.config.depthStencil.depthWriteEnable  = raster.depthWrite ? VK_TRUE : VK_FALSE;
		data.config.depthStencil.depthCompareOp    = ToVk(raster.depthOp);
	}

	if (raster.stencil.enabled)
	{
		data.config.depthStencil.stencilTestEnable = VK_TRUE;
		VkStencilOpState stencil = {
			.failOp      = VK_STENCIL_OP_KEEP,
			.passOp      = ToVk(raster.stencil.passOp),
			.depthFailOp = VK_STENCIL_OP_KEEP,
			.compareOp   = ToVk(raster.stencil.compareOp),
			.compareMask = 0xFF,
			.writeMask   = 0xFF,
			.reference   = raster.stencil.reference
		};
		data.config.depthStencil.front = stencil;
		data.config.depthStencil.back  = stencil;
	}

	data.config.colorAttachmentFormats.clear();
	data.config.colorBlendAttachments.clear();

	for (const auto& format : raster.colorFormats)
	{
		data.config.colorAttachmentFormats.push_back(ToVkFormat(format));

		VkPipelineColorBlendAttachmentState ba = { .colorWriteMask = 0xf };
		if (raster.blend.enabled)
		{
			ba.blendEnable         = VK_TRUE;
			ba.srcColorBlendFactor = ToVk(raster.blend.srcFactor);
			ba.dstColorBlendFactor = ToVk(raster.blend.dstFactor);
			ba.colorBlendOp        = VK_BLEND_OP_ADD;
			ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			ba.alphaBlendOp        = VK_BLEND_OP_ADD;
		}
		data.config.colorBlendAttachments.push_back(ba);
	}

	data.config.renderInfo.colorAttachmentCount    = static_cast<u32>(data.config.colorAttachmentFormats.size());
	data.config.renderInfo.pColorAttachmentFormats = data.config.colorAttachmentFormats.data();
	data.config.renderInfo.depthAttachmentFormat   = data.config.depthAttachmentFormat;

	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetGraphicsStage(GPUShader* vert, GPUShader* frag)
{
	data.shaderStages.stages.clear();

	if (vert)
	{
		const auto v = static_cast<VulkanShader*>(vert);
		const VkPipelineShaderStageCreateInfo vertStage = {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_VERTEX_BIT,
			.module = v->shader,
			.pName  = "vertexMain"
		};
		data.shaderStages.stages.push_back(vertStage);
	}

	if (frag)
	{
		const auto v = static_cast<VulkanShader*>(frag);
		const VkPipelineShaderStageCreateInfo fragStage = {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = v->shader,
			.pName  = "fragmentMain"
		};

		data.shaderStages.stages.push_back(fragStage);
	}

	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetComputeStage(GPUShader* compute)
{
	data.shaderStages.stages.clear();

	if (compute)
	{
		const auto c = static_cast<VulkanShader*>(compute);
		const VkPipelineShaderStageCreateInfo computeStage = {
			.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage  = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = c->shader,
			.pName  = "compMain"
		};
		data.shaderStages.stages.push_back(computeStage);
	}

	return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetVertexInput(const VertexInputLayout& layout)
{
	data.config.bindings.clear();
	data.config.attributes.clear();

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

VulkanPipelineBuilder& VulkanPipelineBuilder::AddPushConstant(const ShaderStageFlags stages, const u32 size, const u32 offset)
{
    if (size == 0) return *this;

    const VkPushConstantRange range = {
        .stageFlags = ToVk(stages),
        .offset = offset,
        .size = size
    };
    data.config.pushConstants.push_back(range);
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::UseLayout(const PipelineLayoutDesc& desc, const VulkanDevice* device)
{
    descriptorSetLayouts.clear();
    for (const auto& setDesc : desc.setLayouts)
    {
        DescriptorLayoutBuilder builder;
        auto layout = builder.AddBindings(setDesc.bindings)
            .Build(device);
        descriptorSetLayouts.push_back(layout.vk);
    }

    data.config.pushConstants.clear();
    for (const auto& pc : desc.pushConstants)
    {
        if (pc.size > device->deviceProperties.limits.maxPushConstantsSize)
        {
            LOG(Error, "Push constant size {} exceeds GPU limit {}", pc.size, device->deviceProperties.limits.maxPushConstantsSize);
        }

        if (pc.size > 0)
        {
            AddPushConstant(pc.stages, pc.size, pc.offset);
        }
    }

    return *this;
}

VkPipelineLayout VulkanPipelineBuilder::BuildLayout(const VulkanDevice* device)
{
    const bool hasPushConstants = !data.config.pushConstants.empty();

    VkPipelineLayoutCreateInfo plInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(descriptorSetLayouts.size()),
        .pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data(),
        .pushConstantRangeCount = hasPushConstants ? static_cast<u32>(data.config.pushConstants.size()) : 0,
        .pPushConstantRanges = hasPushConstants ? data.config.pushConstants.data() : nullptr
    };

    VkPipelineLayout vkLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device->device, &plInfo, nullptr, &vkLayout));
    return vkLayout;
}

Result<VkPipeline> VulkanPipelineBuilder::BuildGraphicsPipeline(const VulkanDevice* device, const VulkanPipelineBuilder& b, VkPipelineLayout layout)
{
	const auto& cfg = b.data.config;
	const auto& stages = b.data.shaderStages.stages;
	auto vertexInputInfo  = MakeVertexInputInfo(b.data.config);
	auto viewportInfo     = MakeViewportInfo();
	auto blendInfo        = MakeBlendInfo(cfg.colorBlendAttachments);
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

	VkPipeline pipeline = VK_NULL_HANDLE;


	VK_CHECK(vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));

	LogPipelineStages(stages);
	return pipeline;
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

VkPipelineColorBlendStateCreateInfo VulkanPipelineBuilder::MakeBlendInfo(const Vector<VkPipelineColorBlendAttachmentState>& attachments)
{
	return {
		.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext           = nullptr,
		.flags           = 0,
		.logicOpEnable   = VK_FALSE,
		.logicOp         = VK_LOGIC_OP_COPY,
		.attachmentCount = static_cast<u32>(attachments.size()),
		.pAttachments    = attachments.data(),
		.blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f }
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

void VulkanGraphicsPipeline::Create()
{
    VulkanPipelineBuilder builder;

    builder.SetGraphicsStage(config.vertexShader, config.fragmentShader)
           .SetVertexInput(config.vertexLayout)
           .ApplyRasterDesc(config.raster)
           .UseLayout(config.layout, device);

    if (vkLayout == VK_NULL_HANDLE)
    {
        vkLayout = builder.BuildLayout(device);
        ownsLayout = true;
    }

    auto result = VulkanPipelineBuilder::BuildGraphicsPipeline(device, builder, vkLayout);
    if (result)
    {
        vkPipeline = result.value();
    }
    else
    {
        LOG(Error, "Failed to create Vulkan Graphics Pipeline");
    }
}

// In VulkanPipeline.cpp
void VulkanPipeline::Destroy()
{
    if (!device || device->device == VK_NULL_HANDLE) return;

    if (vkPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->device, vkPipeline, nullptr);
        vkPipeline = VK_NULL_HANDLE;
    }

    if (vkLayout != VK_NULL_HANDLE && ownsLayout)
    {
        vkDestroyPipelineLayout(device->device, vkLayout, nullptr);
        vkLayout = VK_NULL_HANDLE;
    }

    ownsLayout = false;
}

void VulkanPipeline::Rebuild()
{
    device->WaitIdle();

    const VkPipeline oldPipeline = vkPipeline;
    const VkPipelineLayout oldLayout = vkLayout;
    const bool wasOwner = ownsLayout;

    // Reset to force builder recreation
    vkPipeline = VK_NULL_HANDLE;
    if (ownsLayout) vkLayout = VK_NULL_HANDLE;

    CreateInternal();

    if (vkPipeline != VK_NULL_HANDLE)
    {
        if (oldPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device->device, oldPipeline, nullptr);
        if (oldLayout != VK_NULL_HANDLE && wasOwner) vkDestroyPipelineLayout(device->device, oldLayout, nullptr);
        LOG(Info, "Pipeline Hot-Reloaded successfully.");
    } else {
        // Rollback on failure
        vkPipeline = oldPipeline;
        vkLayout = oldLayout;
        ownsLayout = wasOwner;
        LOG(Warning, "Pipeline Hot-Reload failed! Reverted to working state.");
    }
}

void VulkanComputePipeline::Create()
{
    VulkanPipelineBuilder builder;
    builder.SetComputeStage(config.computeShader)
           .UseLayout(config.layout, device);


    if (vkLayout == VK_NULL_HANDLE)
    {
        vkLayout = builder.BuildLayout(device);
        ownsLayout = true;
    }

    const VkComputePipelineCreateInfo createInfo = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = builder.data.shaderStages.stages[0],
        .layout = this->vkLayout
    };

    VK_CHECK(vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &vkPipeline));
    if (!vkPipeline)
    {
        LOG(Info, "Failed to create Compute Graphics Pipeline");
    }
}