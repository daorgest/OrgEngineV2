//
// Created by Orgest on 6/14/2025.
//

#pragma once
#include "RendererTypes.h"
#include "RenderInterface.h"
#include "VulkanInit.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct DescriptorLayout;
	struct VulkanDevice;

	struct GraphicsPipelineDesc
	{
		GPUShader* vertexShader   = nullptr;
		GPUShader* fragmentShader = nullptr;

		VertexInputLayout vertexLayout;
		GpuRasterDesc raster;
		PipelineLayoutDesc layout;
	};

	struct ComputePipelineDesc
	{
		GPUShader* computeShader = nullptr;
	    PipelineLayoutDesc layout;
	};

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

        ~VulkanPipeline() override
        {
            VulkanPipeline::Destroy();
        }
        void Destroy() override;
        void Rebuild() override;
        [[nodiscard]] bool IsValid() const override { return vkPipeline != VK_NULL_HANDLE; }

        VulkanDevice* device;
        PipelineType type;
        VkPipelineBindPoint bindPoint;
        VkPipeline vkPipeline = VK_NULL_HANDLE;
        VkPipelineLayout vkLayout = VK_NULL_HANDLE;
        bool ownsLayout = false;
    protected:
        virtual void CreateInternal() = 0;
    };

    struct VulkanGraphicsPipeline : VulkanPipeline
    {
        VulkanGraphicsPipeline(VulkanDevice* device, const GraphicsPipelineDesc& inConfig) : VulkanPipeline(device, PipelineType::Graphics), config(inConfig)
        {
            Create();
        };

    protected:
        void CreateInternal() override { Create(); }
        void Create();
    private:
        GraphicsPipelineDesc config;
    };

    struct VulkanComputePipeline : VulkanPipeline
    {
        VulkanComputePipeline(VulkanDevice* device, const ComputePipelineDesc& inConfig) : VulkanPipeline(device, PipelineType::Compute), config(inConfig)
        {
            Create();
        };

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
		VulkanPipelineBuilder& SetGraphicsStage(GPUShader* vert, GPUShader* frag);
		VulkanPipelineBuilder& SetComputeStage(GPUShader* compute);
		VulkanPipelineBuilder& SetVertexInput(const VertexInputLayout& layout);
	    VulkanPipelineBuilder& AddPushConstant(ShaderStageFlags stages, u32 size, u32 offset = 0);
	    VulkanPipelineBuilder& UseLayout(const PipelineLayoutDesc& desc, const VulkanDevice* device);
	    VkPipelineLayout BuildLayout(const VulkanDevice* device);
		static Result<VkPipeline> BuildGraphicsPipeline(const VulkanDevice* device, const VulkanPipelineBuilder& b, VkPipelineLayout layout);


		// Static helpers
		static VkPipelineDynamicStateCreateInfo MakeDynamicStateInfo();
		static VkPipelineViewportStateCreateInfo MakeViewportInfo();
		static VkPipelineColorBlendStateCreateInfo MakeBlendInfo(const Vector<VkPipelineColorBlendAttachmentState>& attachments);
		static VkPipelineVertexInputStateCreateInfo MakeVertexInputInfo(const PipelineConfig& cfg);
		static void LogPipelineStages(const Vector<VkPipelineShaderStageCreateInfo>& stages);
	private:
	    Vector<VkDescriptorSetLayout> descriptorSetLayouts;
	};
} // namespace Renderer
