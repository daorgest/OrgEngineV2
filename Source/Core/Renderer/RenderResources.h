//
// Created by Orgest on 6/5/2026.
//

#pragma once
#include "RenderInterface.h"


namespace Renderer
{
struct SceneRenderTargets {
    Vector<std::unique_ptr<GPUTexture>> targets;
    Extent2D currentExtent = {0, 0};
    SampleCount currentSamples = SampleCount::X1;

    enum TargetIndex { Depth = 0, MSAA = 1, SceneColor = 2 };

    // Interface-based accessors
    GPUTexture* GetDepth() const      { return targets[Depth].get(); }
    GPUTexture* GetMSAA() const       { return targets[MSAA].get(); }
    GPUTexture* GetSceneColor() const { return targets[SceneColor].get(); }

    void Resize(GPUDevice* device, Extent2D extent, SampleCount samples) {
        if (currentExtent.width == extent.width && currentExtent.height == extent.height && currentSamples == samples)
            return;

        device->WaitIdle();

        targets.clear();
        targets.resize(3);

        targets[Depth] = device->CreateTexture(CreateDepthInfo(extent, samples));
        targets[MSAA] = device->CreateTexture(CreateMSAAInfo(extent, samples));
        targets[SceneColor] = device->CreateTexture(CreateColorInfo(extent));

        currentExtent = extent;
        currentSamples = samples;
    }

private:
    // Factory methods return TextureInfo (data, not implementation)
    static TextureInfo CreateDepthInfo(const Extent2D ext, const SampleCount samples) {
        return { .extent = {ext.width, ext.height, 1}, .format = TextureFormat::D32_SFLOAT,
                 .usage = ImageUsage::DepthStencil | ImageUsage::Sampled, .sampleCount = samples };
    }

    static TextureInfo CreateMSAAInfo(const Extent2D ext, const SampleCount samples) {
        return { .extent = {ext.width, ext.height, 1}, .format = TextureFormat::BGRA8_SRGB,
                 .usage = ImageUsage::ColorAttachment | ImageUsage::Transient, .sampleCount = samples };
    }

    static TextureInfo CreateColorInfo(const Extent2D ext) {
        return {
            .extent = {ext.width, ext.height, 1}, .format = TextureFormat::BGRA8_SRGB,
                 .usage = ImageUsage::ColorAttachment | ImageUsage::TransferSrc | ImageUsage::Sampled };
    }
};
}
