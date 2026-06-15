//
// Created by Orgest on 12/20/2025.
//

#include "VulkanDevice.h"

#include "VulkanBuffer.h"
#include "VulkanCheck.h"
#include "VulkanCommandBuffer.h"
#include "VulkanInit.h"
#include "VulkanShader.h"
#include "VulkanShaderBuffer.h"
#include "VulkanTexture.h"
#include "Tools/FileManager.h"

// Realized some of the stuff were core 1.3 so I commened it out to see whats
// was working and what was not
static Vector requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    // VK_EXT_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME,
    // VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME,
    // VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME /// sooon :3
};

using namespace Renderer;

void ImmediateSubmitter::Init(VulkanDevice* inDevice)
{
    assert(device == nullptr && "Device pointer not fond");
    this->device = inDevice;

    cmdBuffer.Init(device);
    immFence.Init(device);
    immFence.Reset();
}

void ImmediateSubmitter::Destroy()
{
    cmdBuffer.Destroy();
    immFence.Destroy();
    device = nullptr;
}

// RHI interface implementation
bool VulkanDevice::Init(GPUInterface* gpuInterface)
{
    // Cast to VulkanInstance (safe because we control the backend)
    auto* vkInstance = static_cast<VulkanInstance*>(gpuInterface);
    return Init(vkInstance);
}

void VulkanDevice::Destroy()
{
    vkDeviceWaitIdle(device);

    immediateSubmitter.Destroy();

    // Pipeline cache saving !!
    size_t cacheSize = 0;
    if (vkGetPipelineCacheData(device, pipelineCache, &cacheSize, nullptr) == VK_SUCCESS && cacheSize > 0)
    {
        Vector<u8> cacheData(cacheSize);
        vkGetPipelineCacheData(device, pipelineCache, &cacheSize, cacheData.data());

        if (const FileManager::Handle file("pipeline_cache.bin", "wb"); file)
        {
            if (const auto writeRes = file.write(Span<const u8>{cacheData}))
            {
                LOG(Info, "Saved Vulkan Pipeline Cache to disk ({} bytes)", cacheSize);
            }
        }
    }

    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
}

SampleCount VulkanDevice::GetMaxUsableSampleCount() const
{
	// Get the intersection of color and depth support
	const VkSampleCountFlags counts = deviceProperties.properties.limits.framebufferColorSampleCounts &
								 deviceProperties.properties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT) return SampleCount::X64;
	if (counts & VK_SAMPLE_COUNT_32_BIT) return SampleCount::X32;
	if (counts & VK_SAMPLE_COUNT_16_BIT) return SampleCount::X16;
	if (counts & VK_SAMPLE_COUNT_8_BIT)  return SampleCount::X8;
	if (counts & VK_SAMPLE_COUNT_4_BIT)  return SampleCount::X4;
	if (counts & VK_SAMPLE_COUNT_2_BIT)  return SampleCount::X2;

	return SampleCount::X1;
}

// Vulkan-specific implementation
bool VulkanDevice::Init(VulkanInstance* inst)
{
    assert(inst != nullptr && "Instance must not be null");
    this->instance = inst;

    u32 deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(inst->instance, &deviceCount, nullptr));
    if (deviceCount < 1)
    {
        LOG(Error, "Failed to find GPU!");
        return false;
    }

    Vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(inst->instance, &deviceCount, devices.data()));

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    u32 bestQueueIndex = ~0u;
    u64 bestScore = 0;

   for (const auto& dev : devices)
    {

        VkPhysicalDeviceProperties2 props2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        vkGetPhysicalDeviceProperties2(dev, &props2);

        auto& props = props2.properties;

        if (props.apiVersion < VK_API_VERSION_1_3)
            continue;

        // check queue families
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        i32 gfxComputeIndex = -1;
        for (u32 i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                gfxComputeIndex = i;
                break;
            }
        }
        if (gfxComputeIndex < 0)
            continue;

        // scoring system to pick best gpu for the job
        u64 score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 2000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 200;
        score += props.limits.maxImageDimension2D / 1024;
        score += (props.driverVersion & 0xFFF); // small tie-breaker

        // VRAM
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(dev, &mp);

        u64 vram = 0;
        for (u32 h = 0; h < mp.memoryHeapCount; ++h)
        {
            if (mp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                if (mp.memoryHeaps[h].size > vram)
                {
                    vram = mp.memoryHeaps[h].size;
                }
            }
        }

        score += vram / 1_GiB;

        if (score > bestScore || bestDevice == VK_NULL_HANDLE)
        {
            bestScore = score;
            bestDevice = dev;
            bestQueueIndex = gfxComputeIndex;
        }
    }

    if (bestDevice == VK_NULL_HANDLE)
    {
        LOG(Error, "Failed to find GPU with Graphics/Compute support!");
        return false;
    }

    physicalDevice = bestDevice;
    graphicsQueueIndex = bestQueueIndex;

    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

    const auto& finalProps = deviceProperties.properties;

    // Recalculate VRAM just for the final UI struct (or store it temporarily in the loop)
    VkPhysicalDeviceMemoryProperties finalMp{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &finalMp);
    u64 finalVram = 0;
    for (u32 h = 0; h < finalMp.memoryHeapCount; ++h) {
        if (finalMp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) finalVram += finalMp.memoryHeaps[h].size;
    }

    // Populate!!
    deviceDesc.name = finalProps.deviceName;
    deviceDesc.vendor = static_cast<GPUVendor>(finalProps.vendorID);
    deviceDesc.type = static_cast<GPUDeviceType>(finalProps.deviceType);
    deviceDesc.driverVersion = finalProps.driverVersion;
    deviceDesc.dedicatedVideoMemory = finalVram;
    deviceDesc.driverVersionString = DecodeDriverVersion(finalProps.driverVersion, deviceDesc.vendor);
    deviceDesc.apiName = fmt::format("Vulkan {}.{}.{}",
                                     VK_VERSION_MAJOR(finalProps.apiVersion),
                                     VK_VERSION_MINOR(finalProps.apiVersion),
                                     VK_VERSION_PATCH(finalProps.apiVersion));

    LOG(Info, "GPU Selected: {}", finalProps.deviceName);

    if (deviceProperties.properties.limits.timestampPeriod == 0.0f)
    {
        LOG(Error, "Timestamp queries not supported on this GPU");
        return false;
    }

    u32 deviceExtensionCount;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr));
    Vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, deviceExtensions.data()));

    bool hasHeapExt = false;
    for (const auto& ext : deviceExtensions)
    {
        if (strcmp(ext.extensionName, VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME) == 0)
        {
            hasHeapExt = true;
            break;
        }
    }

    if (!ValidateVulkanProperties(requiredDeviceExtensions, deviceExtensions, &VkExtensionProperties::extensionName))
    {
        if (!hasHeapExt)
        {
            LOG(Warning, "VK_EXT_descriptor_heap is missing! You must install NVIDIA driver 595.xx or later.");
        }

        LOG(Error, "Required device extensions are missing!!!");
        return false;
    }
    if (hasHeapExt)
    {
        requiredDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
        LOG(Info, "Enabling VK_EXT_descriptor_heap extension.");
    }

    VkPhysicalDeviceDescriptorHeapFeaturesEXT heapFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT,
        .pNext = nullptr
    };

    VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR fifoLatestReadyFeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR,
        .pNext = hasHeapExt ? &heapFeatures : nullptr
    };

    VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedLayoutFeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR,
        .pNext = &fifoLatestReadyFeature
    };

    VkPhysicalDeviceVulkan11Features features11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &unifiedLayoutFeature
    };

    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features11
    };

    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features12
    };

    VkPhysicalDeviceVulkan14Features features14 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = &features13
    };

    VkPhysicalDeviceFeatures2 supportedCore = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features14
    };

    vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedCore);

    VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT
    };

    // Heap Stuff adding into IF it exists
    VkPhysicalDeviceProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = hasHeapExt ? &heapProps : nullptr
    };

    // Populate
    vkGetPhysicalDeviceProperties2(physicalDevice, &props);

    // Check!
    if (!features12.descriptorIndexing ||
        !features12.scalarBlockLayout ||
        !features12.hostQueryReset ||
        !features12.bufferDeviceAddress)
    {
        LOG(Error, "Device missing mandatory Vulkan 1.2 features (Descriptor Indexing, BDA, etc.)!");
        return false;
    }
    if (!features13.dynamicRendering || !features13.synchronization2)
    {
        LOG(Error, "Device missing mandatory Vulkan 1.3 features (Dynamic Rendering, Sync2)!");
        return false;
    }

    if (hasHeapExt)
    {
        deviceDesc.heapProperties = {
            .samplerDescriptorSize = heapProps.samplerDescriptorSize,
            .resourceDescriptorSize = std::max(heapProps.imageDescriptorSize, heapProps.bufferDescriptorSize),
            .samplerReservedSize = heapProps.minSamplerHeapReservedRange,
            .resourceReservedSize = heapProps.minResourceHeapReservedRange,
            .samplerHeapAlignment = 0, // Fallback if missing
            .resourceHeapAlignment = 0 // Fallback if missing
        };
    }


    // this->useUnifiedLayout = (unifiedLayoutFeature.unifiedImageLayouts == VK_TRUE);
    if (this->useUnifiedLayout) LOG(Info, "Vulkan Unified Image Layouts enabled.");
    else LOG(Warning, "Unified Image Layouts not supported. Falling back to legacy transitions.");

    if (fifoLatestReadyFeature.presentModeFifoLatestReady) LOG(Info, "FIFO_LATEST_READY is supported and enabled.");
    else LOG(Warning, "FIFO_LATEST_READY not supported. Falling back to standard FIFO.");

    // graphics queue
    f32 queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphicsQueueIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &supportedCore,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledExtensionCount = static_cast<u32>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
    };

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
    volkLoadDevice(device);
    vkGetDeviceQueue(device, graphicsQueueIndex, 0, &graphicsQueue);

    // VMA allocator
    VmaAllocatorCreateInfo allocatorInfo = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = inst->instance,
        .vulkanApiVersion = inst->appInfo.apiVersion
    };

    VmaVulkanFunctions vulkanFunctions;
    VK_CHECK(vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions));
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator));
    immediateSubmitter.Init(this);

    // Pipeline cache!
    if (!FileManager::CreateIfMissing("pipeline_cache.bin"))
    {
        LOG(Warning, "Could not create pipeline_cache.bin! Check directory permissions.");
    }

    Vector<u8> cacheData;
    if (FileManager::Handle file("pipeline_cache.bin", "rb"); file)
    {
        // Only attempt to read if the file actually has compiled shader data in it
        if (const size_t fileSize = file.size(); fileSize > 0)
        {
            cacheData.resize(fileSize);
            if (file.read<u8>(cacheData))
            {
                LOG(Info, "Loaded Vulkan Pipeline Cache from disk ({} bytes)", fileSize);
            }
        }
        else
        {
            LOG(Info, "Pipeline cache is empty (First boot). Starting fresh.");
        }
    }

    VkPipelineCacheCreateInfo cacheInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = cacheData.size(),
        .pInitialData = cacheData.empty() ? nullptr : cacheData.data()
    };
    VK_CHECK(vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache));


    // MSAA MAX sample count
    const SampleCount maxSamples = GetMaxUsableSampleCount();
    currentSamples = std::min(currentSamples, maxSamples);

    return true;
}

void VulkanDevice::ImmediateSubmit(const std::function<void(GPUCommandBuffer*)> func)
{
    immediateSubmitter.Submit([&](VulkanCommandBuffer* cmd)
    {
        func(cmd);
    }, "RHI Immediate");
}

std::unique_ptr<GPUTexture> VulkanDevice::CreateTexture(const TextureInfo& info)
{
    return std::make_unique<VulkanTexture>(this, info);
}

std::unique_ptr<GPUTextureView> VulkanDevice::CreateTextureView(GPUTexture* texture, const TextureViewInfo& info)
{
    return std::make_unique<VulkanTextureView>(this, static_cast<VulkanTexture*>(texture), info);
}

std::unique_ptr<GPUSampler> VulkanDevice::CreateSampler(SamplerInfo& info)
{
    return std::make_unique<VulkanSampler>(this, info);
}

std::unique_ptr<GPUBuffer> VulkanDevice::CreateBuffer(BufferInfo& info)
{
    return std::make_unique<VulkanBuffer>(this, info);
}

std::shared_ptr<GPUShader> VulkanDevice::CreateShader(Span<const u32> code)
{
    return std::make_shared<VulkanShader>(this, code);
}

std::shared_ptr<GPUShader> VulkanDevice::CreateShaderPath(const char* path)
{
    auto code = FileManager::LoadSPV(path);

    if (!code) {
        LOG(Error, "Failed to create shader from path: {}", path);
        return nullptr;
    }

    return CreateShader(*code);
}

std::unique_ptr<GPUPipeline> VulkanDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    return std::make_unique<VulkanGraphicsPipeline>(this, desc);
}

std::unique_ptr<GPUPipeline> VulkanDevice::CreateComputePipeline(const ComputePipelineDesc& desc)
{
    return std::make_unique<VulkanComputePipeline>(this, desc);
}

std::unique_ptr<GPUShaderBuffer> VulkanDevice::CreateShaderBuffer(DescriptorAllocatorGrowable* alloc,
    const DescriptorSetLayoutDesc& desc)
{
    return std::make_unique<VulkanShaderBuffer>(this, alloc, desc);
}