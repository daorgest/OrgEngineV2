//
// Created by Orgest on 6/14/2025.
//

#include "VulkanPipeline.h"

#include <volk.h>

#include "ShaderCompiler.h"
#include "VulkanCheck.h"
#include "VulkanConvert.h"
#include "VulkanDescriptors.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "Tools/Logger.h"

using namespace Renderer;

VulkanPipelineBuilder::VulkanPipelineBuilder()
{
    data.config.inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    data.config.rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    data.config.multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    data.config.depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    data.config.renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

    data.shaderStages.stages.clear();
}

VulkanPipelineBuilder& VulkanPipelineBuilder::ApplyRasterDesc(const GpuRasterDesc& raster)
{
    data.config.inputAssembly.topology = ToVk(raster.topology);

    data.config.rasterizer.cullMode = ToVk(raster.cull);
    data.config.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    data.config.rasterizer.polygonMode = ToVk(raster.polygonMode);
    data.config.rasterizer.lineWidth = 1.0f;

    data.config.multisampling.rasterizationSamples = ToVk(raster.sampleCount);
    data.config.multisampling.alphaToCoverageEnable = raster.alphaToCoverage ? VK_TRUE : VK_FALSE;

    if (raster.depthFormat != TextureFormat::UNKNOWN)
    {
        data.config.depthAttachmentFormat = ToVkFormat(raster.depthFormat);

        auto& ds = data.config.depthStencil;
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = raster.depthWrite ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp = ToVk(raster.depthOp);

        if (raster.stencil.enabled)
        {
            ds.stencilTestEnable = VK_TRUE;
            VkStencilOpState stencil = {
                .failOp = VK_STENCIL_OP_KEEP,
                .passOp = ToVk(raster.stencil.passOp),
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp = ToVk(raster.stencil.compareOp),
                .compareMask = 0xFF,
                .writeMask = 0xFF,
                .reference = raster.stencil.reference
            };
            ds.front = stencil;
            ds.back = stencil;
        }
    }


    data.config.colorAttachmentFormats.clear();
    data.config.colorBlendAttachments.clear();

    for (const auto& format : raster.colorFormats)
    {
        data.config.colorAttachmentFormats.push_back(ToVkFormat(format));

        VkPipelineColorBlendAttachmentState ba = {.colorWriteMask = 0xf};
        if (raster.blend.enabled)
        {
            ba.blendEnable = VK_TRUE;
            ba.srcColorBlendFactor = ToVk(raster.blend.srcFactor);
            ba.dstColorBlendFactor = ToVk(raster.blend.dstFactor);
            ba.colorBlendOp = VK_BLEND_OP_ADD;
            ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            ba.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        data.config.colorBlendAttachments.push_back(ba);
    }

    data.config.renderInfo.colorAttachmentCount = static_cast<u32>(data.config.colorAttachmentFormats.size());
    data.config.renderInfo.pColorAttachmentFormats = data.config.colorAttachmentFormats.data();
    data.config.renderInfo.depthAttachmentFormat = data.config.depthAttachmentFormat;

    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetGraphicsStage(const std::shared_ptr<GPUShader>& vert,
                                                               const std::shared_ptr<GPUShader>& frag)
{
    data.shaderStages.stages.clear();

    if (vert)
    {
        const auto v = static_cast<VulkanShader*>(vert.get());
        const VkPipelineShaderStageCreateInfo vertStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = v->shader,
            .pName = "vertexMain"
        };
        data.shaderStages.stages.push_back(vertStage);
    }

    if (frag)
    {
        const auto v = static_cast<VulkanShader*>(frag.get());
        const VkPipelineShaderStageCreateInfo fragStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = v->shader,
            .pName = "fragmentMain"
        };
        data.shaderStages.stages.push_back(fragStage);
    }

    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::SetComputeStage(const std::shared_ptr<GPUShader>& compute)
{
    data.shaderStages.stages.clear();

    if (compute)
    {
        const auto c = static_cast<VulkanShader*>(compute.get());
        const VkPipelineShaderStageCreateInfo computeStage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = c->shader,
            .pName = "compMain"
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

VulkanPipelineBuilder& VulkanPipelineBuilder::AddPushConstant(const ShaderStageFlags stages, const u32 size,
                                                              const u32 offset)
{
    if (size > 0)
    {
        data.config.pushConstants.push_back({
            .stageFlags = ToVk(stages),
            .offset = offset,
            .size = size
        });
    }
    return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::UseLayout(const PipelineLayoutDesc& desc, const VulkanDevice* device, Vector<VkDescriptorSetLayout>& outLayouts)
{
    if (data.config.layoutDesc == desc) return *this;
    data.config.layoutDesc = desc;

    if (desc.setLayouts.empty()) return *this;

    u32 maxSet = 0;
    for (const auto& [setIndex, bindings] : desc.setLayouts)
    {
        if (setIndex > maxSet)
        {
            maxSet = setIndex;
        }
    }

    outLayouts.clear();
    outLayouts.assign(maxSet + 1, VK_NULL_HANDLE);

    for (const auto& setDesc : desc.setLayouts)
    {
        DescriptorLayoutBuilder builder;
        outLayouts[setDesc.setIndex] = builder.BuildFromDesc(device, setDesc).vk;
    }

    data.config.pushConstants.clear();
    for (const auto& [size, offset, stages] : desc.pushConstants)
    {
        if (size > 0 && size <= device->deviceProperties.properties.limits.maxPushConstantsSize)
        {
            AddPushConstant(stages, size, offset);
        }
    }

    return *this;
}

VkPipelineLayout VulkanPipelineBuilder::BuildLayout(const VulkanDevice* device, const Vector<VkDescriptorSetLayout>& layouts)
{
    const bool hasPushConstants = !data.config.pushConstants.empty();

    VkPipelineLayoutCreateInfo plInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<u32>(layouts.size()),
        .pSetLayouts = layouts.empty() ? nullptr : layouts.data(),
        .pushConstantRangeCount = hasPushConstants ? static_cast<u32>(data.config.pushConstants.size()) : 0,
        .pPushConstantRanges = hasPushConstants ? data.config.pushConstants.data() : nullptr
    };

    VkPipelineLayout vkLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device->device, &plInfo, nullptr, &vkLayout));

    return vkLayout;
}

Result<VkPipeline> VulkanPipelineBuilder::BuildGraphicsPipeline(const VulkanDevice* device,
                                                                const VulkanPipelineBuilder& b, VkPipelineLayout layout)
{
    const auto& cfg = b.data.config;
    const auto& stages = b.data.shaderStages.stages;

    auto vertexInputInfo = MakeVertexInputInfo(cfg);
    auto viewportInfo = MakeViewportInfo();
    auto blendInfo = MakeBlendInfo(cfg.colorBlendAttachments);
    auto dynamicStateInfo = MakeDynamicStateInfo();

    VkGraphicsPipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &cfg.renderInfo,
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &cfg.inputAssembly,
        .pViewportState = &viewportInfo,
        .pRasterizationState = &cfg.rasterizer,
        .pMultisampleState = &cfg.multisampling,
        .pDepthStencilState = &cfg.depthStencil,
        .pColorBlendState = &blendInfo,
        .pDynamicState = &dynamicStateInfo,
        .layout = layout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device->device, device->pipelineCache, 1, &info, nullptr, &pipeline));

    LogPipelineStages(stages);
    return pipeline;
}

Result<VkPipeline> VulkanPipelineBuilder::BuildComputePipeline(const VulkanDevice* device,
                                                               const VulkanPipelineBuilder& b, VkPipelineLayout layout)
{
    const auto& stage = b.data.shaderStages.stages[0]; //

    const VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(device->device, device->pipelineCache, 1, &info, nullptr, &pipeline));
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

VkPipelineColorBlendStateCreateInfo VulkanPipelineBuilder::MakeBlendInfo(
    const Vector<VkPipelineColorBlendAttachmentState>& attachments)
{
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = static_cast<u32>(attachments.size()),
        .pAttachments = attachments.data(),
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
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
            (stage.stage == VK_SHADER_STAGE_VERTEX_BIT)
                ? "VERTEX"
                : (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
                ? "FRAGMENT"
                : (stage.stage == VK_SHADER_STAGE_COMPUTE_BIT)
                ? "COMPUTE"
                : "OTHER";

        LOG(Info, "  Stage {}: {} (entry: {})", i, name, stage.pName);
    }
}

void VulkanGraphicsPipeline::Create()
{
    VulkanPipelineBuilder builder;

    builder.SetGraphicsStage(config.vertexShader, config.fragmentShader)
           .SetVertexInput(config.vertexLayout)
           .ApplyRasterDesc(config.raster);

    if (vkLayout == VK_NULL_HANDLE)
    {
        builder.UseLayout(config.layout, device, descriptorSetLayouts);
        vkLayout = builder.BuildLayout(device, descriptorSetLayouts);
        ownsLayout = true;
    }

    if (const auto result = VulkanPipelineBuilder::BuildGraphicsPipeline(device, builder, vkLayout))
        vkPipeline = result.value();

    else LOG(Error, "Failed to create Vulkan Graphics Pipeline");
}


void VulkanPipeline::Destroy()
{
    if (!device || device->device == VK_NULL_HANDLE) return;

    if (vkPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->device, vkPipeline, nullptr);
        vkPipeline = VK_NULL_HANDLE;
    }

    if (ownsLayout)
    {
        if (vkLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device->device, vkLayout, nullptr);
            vkLayout = VK_NULL_HANDLE;
        }

        for (const auto& layout : descriptorSetLayouts)
        {
            if (layout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(device->device, layout, nullptr);
            }
        }
    }

    descriptorSetLayouts.clear();
    ownsLayout = false;
}

void VulkanPipeline::Rebuild()
{
    if (vkPipeline != VK_NULL_HANDLE)
    {
        device->WaitIdle();
        vkDestroyPipeline(device->device, vkPipeline, nullptr);
        vkPipeline = VK_NULL_HANDLE;
    }

    CreateInternal();
}

void VulkanComputePipeline::Create()
{

    VulkanPipelineBuilder builder;
    builder.SetComputeStage(config.computeShader);

    if (vkLayout == VK_NULL_HANDLE)
    {
        builder.UseLayout(config.layout, device, descriptorSetLayouts);
        vkLayout = builder.BuildLayout(device, descriptorSetLayouts);
        ownsLayout = true;
    }

    if (const auto result = VulkanPipelineBuilder::BuildComputePipeline(device, builder, vkLayout))
        vkPipeline = result.value();

    else LOG(Error, "Failed to create Vulkan Compute Pipeline");
}

std::string_view VulkanPipeline::GetSourcePath() const
{
    if (type == PipelineType::Graphics)
    {
        return static_cast<const VulkanGraphicsPipeline*>(this)->config.slangSourcePath;
    }
    if (type == PipelineType::Compute)
    {
        return static_cast<const VulkanComputePipeline*>(this)->config.slangSourcePath;
    }
    return "";
}

void VulkanPipeline::ApplyReload(const CompileResult& result)
{
    LOG(Info, "[Apply State] Attempting reload for 0x{}", static_cast<void*>(this));

    auto swapState = [this](const std::shared_ptr<GPUShader>& newShader) -> std::shared_ptr<GPUShader>
    {
        if (type == PipelineType::Graphics)
        {
            auto* gfx = static_cast<VulkanGraphicsPipeline*>(this);
            auto old = gfx->config.vertexShader;
            gfx->config.vertexShader = newShader;
            gfx->config.fragmentShader = newShader;
            return old;
        }

        // Compute
        auto* comp = static_cast<VulkanComputePipeline*>(this);
        auto old = comp->config.computeShader;
        comp->config.computeShader = newShader;
        return old;
    };

    const std::shared_ptr<GPUShader> newShader = device->CreateShader(result.code);
    const std::shared_ptr<GPUShader> oldShader = swapState(newShader);

    this->Rebuild();

    if (this->vkPipeline == VK_NULL_HANDLE)
    {
        LOG(Error, "Hot-Reload failed to build Vulkan pipeline. Reverting to old shader logic.");
        swapState(oldShader);
        this->Rebuild();
    }
}
