//
// Created by Orgest on 6/14/2025.
//

#pragma once
#include <span>
#include <string>
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

		// Hot reload support
		bool enableHotReload = false;
		std::string shaderSourcePath;  // Path to .slang file
		bool isComputeShader = false;
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
		VulkanPipelineBuilder& SetFragVerShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
		VulkanPipelineBuilder& SetComputeShader(VkShaderModule computeShader);
		VulkanPipelineBuilder& SetInputTopology(VkPrimitiveTopology topology);
		VulkanPipelineBuilder& SetVertexInput(std::span<const VkVertexInputBindingDescription> bindings,
			std::span<const VkVertexInputAttributeDescription> attributes);
		VulkanPipelineBuilder& SetVertexInput(const VertexInputLayout& layout); // RHI
		VulkanPipelineBuilder& SetPushConstantRange(u32 size, VkShaderStageFlags stageFlags = VK_SHADER_STAGE_VERTEX_BIT, u32 offset = 0);
		VulkanPipelineBuilder& SetPolygonMode(VkPolygonMode mode);
		VulkanPipelineBuilder& SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
		VulkanPipelineBuilder& EnableMultisampling(VkSampleCountFlagBits sampleCount);
		VulkanPipelineBuilder& SetMultisamplingNone();
		VulkanPipelineBuilder& DisableBlending();
		VulkanPipelineBuilder& SetColorAttachmentFormat(TextureFormat format);
		VulkanPipelineBuilder& SetDepthAttachmentFormat(TextureFormat format);
		VulkanPipelineBuilder& SetDepthFormat(TextureFormat format);
		VulkanPipelineBuilder& DisableDepthTest();
		VulkanPipelineBuilder& EnableDepthTest(bool depthWriteEnable, VkCompareOp op);
		VulkanPipelineBuilder& EnableBlendingAdditive();
		VulkanPipelineBuilder& EnableBlendingAlphaBlend();
		VulkanPipelineBuilder& Layout(const VkPipelineLayout& layout);

		// Hot reload support (soon)
		VulkanPipelineBuilder& EnableHotReload(const char* shaderSourcePath, bool isCompute = false);


		// Static helpers
		static VkPipelineDynamicStateCreateInfo MakeDynamicStateInfo();
		static VkPipelineViewportStateCreateInfo MakeViewportInfo();
		static VkPipelineColorBlendStateCreateInfo MakeBlendInfo(const VkPipelineColorBlendAttachmentState& src);
		static VkPipelineVertexInputStateCreateInfo MakeVertexInputInfo(const PipelineConfig& cfg);
		static void LogPipelineStages(const Vector<VkPipelineShaderStageCreateInfo>& stages);
		static Result<VkPipeline> BuildGraphicsPipeline(const VulkanDevice* device, const VulkanPipelineBuilder& b, VkPipelineLayout layout);


	};

	struct PipelineLayoutDesc
	{
		std::span<const VkDescriptorSetLayout> setLayouts;
		std::span<const VkPushConstantRange>   pushRanges;
	};

	/// Vulkan implementation of GPUPipeline
	struct VulkanPipeline final : GPUPipeline
	{
		// RHI interface implementation
		void Destroy() override;
		[[nodiscard]] bool IsValid() const override { return vk != VK_NULL_HANDLE; }
		void Rebuild(GPUDevice* device) override;

		// Vulkan-specific
		[[nodiscard]] Result<bool> Create(VulkanDevice* device, const PipelineLayoutDesc& layoutDesc,
		                                   const VulkanPipelineBuilder& builder);

		[[nodiscard]] VkPipeline GetVkPipeline() const noexcept { return vk; }
		[[nodiscard]] VkPipelineLayout GetVkLayout() const noexcept { return vkLayout; }
		explicit operator VkPipeline() const noexcept { return vk; }


		VulkanPipeline() = default;
		explicit VulkanPipeline(VulkanDevice* device) : device(device) {}
		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;

		~VulkanPipeline() override { Destroy(); }

		VkPipeline vk = VK_NULL_HANDLE;
		VkPipelineLayout vkLayout = VK_NULL_HANDLE;

	private:
		VulkanDevice* device = nullptr; // Non-const for shader creation during rebuild
		bool ownsLayout = true;
		PipelineLayoutDesc layoutDesc;
		VulkanPipelineBuilder builder;
	};

} // namespace Renderer
