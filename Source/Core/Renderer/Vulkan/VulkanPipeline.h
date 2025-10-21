//
// Created by Orgest on 6/14/2025.
//

#pragma once
#include <span>
#include <volk.h>

#include "RendererTypes.h"
#include "RenderInterface.h"
#include "VulkanInit.h"
#include "Tools/Vector.h"

namespace Renderer
{
	struct ShaderStages
	{
		Vector<VkPipelineShaderStageCreateInfo> stages;
	};

	struct PipelineConfig
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		VkPipelineRenderingCreateInfo renderInfo = {};
		VkFormat colorAttachmentFormat = VK_FORMAT_UNDEFINED;
		VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
		// for now
		Vector<VkVertexInputBindingDescription> bindings;
		Vector<VkVertexInputAttributeDescription> attributes;
		VkPushConstantRange pushConstantRange = {};
		bool hasPushConstant = false;
	};

	struct PipelineData
	{
		ShaderStages shaderStages;
		PipelineConfig config;
	};


	struct VulkanPipeline;
	class VulkanPipelineBuilder
	{
	public:
		PipelineData data;

		VulkanPipelineBuilder();

		VulkanPipeline BuildPipeline(VulkanDevice* device);
		VulkanPipelineBuilder& SetFragVerShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
		VulkanPipelineBuilder& SetComputeShader(VkShaderModule computeShader);
		VulkanPipelineBuilder& SetInputTopology(VkPrimitiveTopology topology);
		VulkanPipelineBuilder& SetVertexInput(std::span<const VkVertexInputBindingDescription> bindings,
			std::span<const VkVertexInputAttributeDescription> attributes);
		VulkanPipelineBuilder& SetPushConstantRange(u32 size, VkShaderStageFlags stageFlags = VK_SHADER_STAGE_VERTEX_BIT, u32 offset = 0);
		VulkanPipelineBuilder& SetPolygonMode(VkPolygonMode mode);
		VulkanPipelineBuilder& SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
		VulkanPipelineBuilder& EnableMultisampling(VkSampleCountFlagBits sampleCount);
		VulkanPipelineBuilder& SetMultisamplingNone();
		VulkanPipelineBuilder& DisableBlending();
		VulkanPipelineBuilder& SetColorAttachmentFormat(VkFormat format);
		VulkanPipelineBuilder& SetDepthAttachmentFormat(VkFormat format);
		VulkanPipelineBuilder& SetDepthFormat(VkFormat format);
		VulkanPipelineBuilder& DisableDepthTest();
		VulkanPipelineBuilder& EnableDepthTest(bool depthWriteEnable, VkCompareOp op);
		VulkanPipelineBuilder& EnableBlendingAdditive();
		VulkanPipelineBuilder& EnableBlendingAlphaBlend();
		VulkanPipelineBuilder& Layout(const VkPipelineLayout& layout);

	};

	struct PipelineLayoutDesc
	{
		std::span<const VkDescriptorSetLayout> setLayouts;
		std::span<const VkPushConstantRange>   pushRanges;
	};

	struct VulkanPipeline
	{
		VkPipeline vk = VK_NULL_HANDLE;
		VkPipelineLayout vkLayout = VK_NULL_HANDLE;
		const VulkanDevice* device = nullptr;

		VulkanPipeline() = default;
		explicit VulkanPipeline(VulkanDevice* device) : device(device) {};

		operator VkPipeline() const noexcept { return vk; }

		~VulkanPipeline() { Destroy();}

		void Destroy() const;
		[[nodiscard]] Result<bool> Create(const VulkanDevice * device, const PipelineLayoutDesc & layoutDesc, const VulkanPipelineBuilder & builder);
	};

};
