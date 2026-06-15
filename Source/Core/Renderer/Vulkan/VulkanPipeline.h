//
// Created by Orgest on 6/14/2025.
//

#pragma once
#include "RendererTypes.h"
#include "RenderInterface.h"
#include "ShaderCompiler.h"
#include "VulkanInit.h"
#include "Tools/Vector.h"

namespace Renderer
{
    struct DescriptorLayout;
    struct VulkanDevice;

    struct VulkanPipeline : GPUPipeline
    {
        VulkanPipeline(VulkanDevice* device, const PipelineType type) : device(device), type(type)
        {
            switch (type)
            {
            case PipelineType::Graphics:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case PipelineType::Compute:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            case PipelineType::Raytracing:
                bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
                break;
            default:
                bindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
                break;
            }
        }

        ~VulkanPipeline() override { VulkanPipeline::Destroy(); };

        void Rebuild() override;
        void Destroy() override;

        // Yeah yeah....
        explicit constexpr operator bool() const noexcept override { return vkPipeline != VK_NULL_HANDLE; };
        [[nodiscard]] std::string_view GetSourcePath() const;
        [[nodiscard]] const PipelineLayoutDesc& GetLayoutDesc() const override = 0;

        void ApplyReload(const CompileResult& result);

        VulkanDevice* device = nullptr;
        PipelineType type;
        VkPipelineBindPoint bindPoint;
        VkPipeline vkPipeline = VK_NULL_HANDLE;
        VkPipelineLayout vkLayout = VK_NULL_HANDLE;
        Vector<VkDescriptorSetLayout> descriptorSetLayouts;
        bool ownsLayout = false;

    protected:
        virtual void CreateInternal() = 0;
    };

    struct VulkanGraphicsPipeline final : VulkanPipeline
    {
        friend struct VulkanPipeline;

        VulkanGraphicsPipeline(VulkanDevice* device, const GraphicsPipelineDesc& inConfig) :
            VulkanPipeline(device, PipelineType::Graphics), config(inConfig)
        {
            Create();
        };

        void SetSampleCountAndRebuild(const SampleCount samples) override
        {
            if (config.raster.sampleCount == samples) return;

            config.raster.sampleCount = samples;
            Rebuild();
        }
        [[nodiscard]] const PipelineLayoutDesc& GetLayoutDesc() const override { return config.layout; };
        [[nodiscard]] GraphicsPipelineDesc& GetPipelineConfig() { return config; }

    protected:
        void CreateInternal() override { Create(); }
        void Create();

    private:
        GraphicsPipelineDesc config;
    };

    struct VulkanComputePipeline final : VulkanPipeline
    {
        friend struct VulkanPipeline;

        VulkanComputePipeline(VulkanDevice* device, const ComputePipelineDesc& inConfig) :
                        VulkanPipeline(device, PipelineType::Compute), config(inConfig)
        {
            Create();
        }

        void SetSampleCountAndRebuild(SampleCount) override {};
        [[nodiscard]] const PipelineLayoutDesc& GetLayoutDesc() const override { return config.layout; };

    protected:
        void CreateInternal() override { Create(); }
        void Create();

    private:
        ComputePipelineDesc config;
    };

    struct ShaderStages
    {
        Vector<VkPipelineShaderStageCreateInfo> stages;
    };

    struct PipelineConfig
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        VkPipelineRasterizationStateCreateInfo rasterizer = {};

        Vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        Vector<VkFormat> colorAttachmentFormats;
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        VkPipelineRenderingCreateInfo renderInfo = {};

        Vector<VkVertexInputBindingDescription> bindings;
        Vector<VkVertexInputAttributeDescription> attributes;
        Vector<VkPushConstantRange> pushConstants;
        Vector<VkDescriptorSetAndBindingMappingEXT> vkMappings;
        PipelineLayoutDesc layoutDesc;
    };

    class VulkanPipelineBuilder
    {
    public:
        struct PipelineData
        {
            ShaderStages shaderStages;
            PipelineConfig config;
        } data;

        VulkanPipelineBuilder();


        VulkanPipelineBuilder& ApplyRasterDesc(const GpuRasterDesc& raster);
        VulkanPipelineBuilder& SetGraphicsStage(const std::shared_ptr<GPUShader>& vert,
                                                const std::shared_ptr<GPUShader>& frag);
        VulkanPipelineBuilder& SetComputeStage(const std::shared_ptr<GPUShader>& compute);
        VulkanPipelineBuilder& SetVertexInput(const VertexInputLayout& layout);
        VulkanPipelineBuilder& AddPushConstant(ShaderStageFlags stages, u32 size, u32 offset = 0);
        VulkanPipelineBuilder& UseLayout(const PipelineLayoutDesc& desc, const VulkanDevice* device, Vector<VkDescriptorSetLayout>& outLayouts);
        VkPipelineLayout BuildLayout(const VulkanDevice* device, const Vector<VkDescriptorSetLayout>& layouts);
        static Result<VkPipeline> BuildGraphicsPipeline(const VulkanDevice* device, const VulkanPipelineBuilder& b, VkPipelineLayout layout);
        static Result<VkPipeline> BuildComputePipeline(const VulkanDevice* device, const VulkanPipelineBuilder& b, VkPipelineLayout layout);

        // Static helpers
        static VkPipelineDynamicStateCreateInfo MakeDynamicStateInfo();
        static VkPipelineViewportStateCreateInfo MakeViewportInfo();
        static VkPipelineColorBlendStateCreateInfo MakeBlendInfo(
            const Vector<VkPipelineColorBlendAttachmentState>& attachments);
        static VkPipelineVertexInputStateCreateInfo MakeVertexInputInfo(const PipelineConfig& cfg);
        static void LogPipelineStages(const Vector<VkPipelineShaderStageCreateInfo>& stages);
    };
} // namespace Renderer
