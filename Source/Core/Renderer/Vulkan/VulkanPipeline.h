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

	struct ShaderStages
	{
		Vector<VkPipelineShaderStageCreateInfo> stages;
	};


	struct GraphicsPipelineDesc
	{
		// Shaders stored as nullable pointers
		GPUShader* vertexShader   = nullptr;
		GPUShader* fragmentShader = nullptr;

		VertexInputLayout vertexLayout;
		GpuRasterDesc raster;

		PipelineLayoutDesc layout;
		VkPipelineLayout externalLayout = VK_NULL_HANDLE;
	};

	struct ComputePipelineDesc
	{
		// Single compute shader pointer
		GPUShader* computeShader = nullptr;
		PipelineLayoutDesc layout;
		VkPipelineLayout externalLayout = VK_NULL_HANDLE;
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

		// for now
		Vector<VkVertexInputBindingDescription> bindings;
		Vector<VkVertexInputAttributeDescription> attributes;
		VkPushConstantRange pushConstantRange = {};
		bool hasPushConstant = false;
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
		// New Shit
		VulkanPipelineBuilder& ApplyRasterDesc(const GpuRasterDesc& raster);
		VulkanPipelineBuilder& SetGraphicsStage(GPUShader* vert, GPUShader* frag);
		VulkanPipelineBuilder& SetComputeStage(GPUShader* compute);
		VulkanPipelineBuilder& SetVertexInput(const VertexInputLayout& layout);
		VulkanPipelineBuilder& Layout(VkPipelineLayout layout);

		static VkPipelineLayout CreateLayout(const VulkanDevice* device, const PipelineLayoutDesc& desc);
		static Result<VkPipeline> BuildGraphicsPipeline(const VulkanDevice* device, const VulkanPipelineBuilder& b, VkPipelineLayout layout);


		// Static helpers
		static VkPipelineDynamicStateCreateInfo MakeDynamicStateInfo();
		static VkPipelineViewportStateCreateInfo MakeViewportInfo();
		static VkPipelineColorBlendStateCreateInfo MakeBlendInfo(const Vector<VkPipelineColorBlendAttachmentState>& attachments);
		static VkPipelineVertexInputStateCreateInfo MakeVertexInputInfo(const PipelineConfig& cfg);
		static void LogPipelineStages(const Vector<VkPipelineShaderStageCreateInfo>& stages);

	};

	/// Vulkan implementation of GPUPipeline
	struct VulkanPipeline final : GPUPipeline
	{
		void Destroy() override;
		[[nodiscard]] bool IsValid() const override { return vk != VK_NULL_HANDLE; }
		void Rebuild(GPUDevice* device) override;

		Result<void> CreateGraphicsPipeline(VulkanDevice* inDevice, const GraphicsPipelineDesc& desc);
		Result<void> CreateComputePipeline(VulkanDevice* device, ComputePipelineDesc& desc);

		[[nodiscard]] VkPipeline GetVkPipeline() const noexcept { return vk; }
		[[nodiscard]] VkPipelineLayout GetVkLayout() const noexcept { return vkLayout; }

		VulkanPipeline() = default;

		explicit VulkanPipeline(VulkanDevice* device) : device(device){}

		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;

		VulkanPipeline(VulkanPipeline&& other) noexcept
		{
			*this = std::move(other);
		}

		VulkanPipeline& operator=(VulkanPipeline&& other) noexcept
		{
			if (this != &other)
			{
				Destroy();
				vk = other.vk;
				vkLayout = other.vkLayout;
				ownsLayout = other.ownsLayout;

				layoutMetadata = std::move(other.layoutMetadata);
				other.vk = VK_NULL_HANDLE;
				other.vkLayout = VK_NULL_HANDLE;
			}
			return *this;
		}

		~VulkanPipeline() override { Destroy(); }

		VkPipeline vk = VK_NULL_HANDLE;
		VkPipelineLayout vkLayout = VK_NULL_HANDLE;
		Vector<DescriptorSetLayoutDesc> layoutMetadata;

	private:
		VulkanDevice* device = nullptr; // Non-const for shader creation during rebuild
		bool ownsLayout = true;
	};

} // namespace Renderer
