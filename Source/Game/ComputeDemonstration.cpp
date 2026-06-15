//
// Created by Orgest on 1/30/2026.
//

#include "ComputeDemonstration.h"

#include "ShaderConstants.h"
#include "VulkanImGuiHelpets.h"
#include "Tools/Logger.h"

using namespace Renderer;

void ComputeDemonstration::Init(Renderer::GPUDevice* device, Renderer::GPUShaderManager* shaderManager, Renderer::DescriptorAllocatorGrowable& descAlloc)
{
    shader = device->CreateShaderPath("Shaders/gradiant_comp.spv");

    DescriptorLayoutBuilder builder;
    layout = builder.AddBindings(Constants::Compute).Build(device);
    descriptorSet = std::make_unique<VulkanDescriptorSet>(descAlloc.Allocate(layout));

    ComputePipelineDesc desc = {
        .computeShader = shader,
        // .slangSourcePath ="Shaders/gradiant_comp.slang"
    };

    desc.layout.setLayouts.push_back(DescriptorSetLayoutDesc::FromConstants(0, Constants::Compute));
    desc.layout.pushConstants.push_back({sizeof(ComputePushConstants), 0, Renderer::ShaderStage::Compute});
    pipeline = device->CreateComputePipeline(desc);

    if (!pipeline)
    {
        LOG(Error, "Failed to build demonstration compute pipeline!");
        return;
    }
    // shaderManager->RegisterPipeline(pipeline.get());

    Resize(device, {1280, 720});
}



void ComputeDemonstration::Resize(GPUDevice* device, const Extent2D& newSize)
{
    if (newSize.width == lastExtent.width && newSize.height == lastExtent.height) return;
    if (newSize.width == 0 || newSize.height == 0) return;

    device->WaitIdle();

    const TextureInfo textureInfo = {
        .extent = {newSize.width, newSize.height},
        .format = TextureFormat::RGBA8_UNORM,
        .usage = ImageUsage::Sampled | ImageUsage::Storage
    };

    texture = device->CreateTexture(textureInfo);
    texture->SetName("Compute Output");

    SamplerInfo samplerInfo;
    displaySampler = device->CreateSampler(samplerInfo);

    descriptorSet->WriteTexture(0, texture.get()->GetView(), displaySampler.get(),
                                Renderer::DescriptorType::StorageImage);
    descriptorSet->Update(device);

    if (texId != ImTextureID_Invalid)
    {
        UI::RemoveTexture(texId);
    }

    texId = UI::AddTexture(texture.get()->GetView(), TextureLayout::General);

    lastExtent = newSize;
}

void ComputeDemonstration::Destroy()
{
    if (texId != ImTextureID_Invalid)
    {
        UI::RemoveTexture(texId);
        texId = ImTextureID_Invalid;
    }
}

void ComputeDemonstration::Execute(GPUCommandBuffer* cmd, const Extent2D extent, const ComputePushConstants& cPC) const
{
    cmd->TransitionLayout(texture.get(), TextureLayout::ShaderReadOnly);
    cmd->FlushBarriers();
    cmd->BindPipeline(pipeline.get());
    cmd->BindDescriptorSet(descriptorSet.get(), 0, pipeline.get());

    cmd->PushConstants(pipeline.get(), ShaderStage::Compute, 0, sizeof(ComputePushConstants), &cPC);

    const u32 groupX = static_cast<u32>(std::ceil(extent.width / 16.0f));
    const u32 groupY = static_cast<u32>(std::ceil(extent.height / 16.0f));
    cmd->Dispatch(groupX, groupY, 1);
}

void ComputeDemonstration::DrawUI(GPUDevice* device)
{


    // --- 1. DEFAULT POSITIONING LOGIC ---
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const f32 fontSize = ImGui::GetFontSize();

    // The main overlay scales dynamically up to ~24 font units wide.
    // We anchor this window just to the right of it, with a tiny gap.
    const ImVec2 defaultPos = { vp->WorkPos.x + (26.0f * fontSize), vp->WorkPos.y + (0.5f * fontSize) };

    // A nice default 16:9 starting resolution (e.g., 640x360)
    constexpr ImVec2 defaultSize = { 640.0f, 360.0f };


    ImGui::SetNextWindowPos(defaultPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const std::string windowTitle = fmt::format("Compute Shader Output: {}x{}###ComputeWindow", lastExtent.width, lastExtent.height);
    ImGui::Begin(windowTitle.c_str());

    const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x > 0.0f && viewportSize.y > 0.0f)
    {

        Resize(device, { static_cast<u32>(viewportSize.x), static_cast<u32>(viewportSize.y) });
        if (texId != ImTextureID_Invalid)
        {
            ImGui::Image(texId, viewportSize);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
