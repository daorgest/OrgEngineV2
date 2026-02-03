//
// Created by Orgest on 12/20/2025.
//

#include "VulkanDevice.h"

#include "VulkanBuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanInit.h"
#include "VulkanShader.h"
#include "Tools/Array.h"
#include "Tools/FileManager.h"

static Array requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME,
    VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME
};

using namespace Renderer;

// RHI interface implementation
bool VulkanDevice::Init(GPUInterface* gpuInterface)
{
    // Cast to VulkanInstance (safe because we control the backend)
    auto* vkInstance = static_cast<VulkanInstance*>(gpuInterface);
    return Init(vkInstance);
}

// Vulkan-specific implementation
bool VulkanDevice::Init(VulkanInstance* inst)
{
    this->instance = inst;

    u32 deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(inst->instance, &deviceCount, nullptr));
    if (deviceCount == 0)
    {
        LOG(Error, "Failed to find GPU!");
        return false;
    }

    Vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(inst->instance, &deviceCount, devices.data());

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    u32 bestQueueIndex = ~0u;
    u64 bestScore = 0;

    for (const auto& dev : devices)
    {
        // let's check for min Vulkan 1.3 support, and store physical device properties
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(dev, &props);
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
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
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

        if (score > bestScore || bestDevice == VK_NULL_HANDLE)
        {
            VkPhysicalDeviceMemoryProperties mp{};
            vkGetPhysicalDeviceMemoryProperties(dev, &mp);

            u64 vram = 0;
            for (u32 h = 0; h < mp.memoryHeapCount; ++h)
            {
                if (mp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    vram += mp.memoryHeaps[h].size;
            }

            score += vram / (128ull * 1024ull * 1024);

            // Final check with VRAM included
            if (score > bestScore)
            {
                bestScore = score;
                bestDevice = dev;
                bestQueueIndex = gfxComputeIndex;

                LOG(Info, "GPU Selected: {}", props.deviceName);

                // Fill descriptor for UI
                deviceDesc.name = props.deviceName;
                deviceDesc.vendor = static_cast<GPUVendor>(props.vendorID);
                deviceDesc.type = static_cast<GPUDeviceType>(props.deviceType);
                deviceDesc.driverVersion = props.driverVersion;
                deviceDesc.dedicatedVideoMemory = vram;
                deviceDesc.driverVersionString = DecodeDriverVersion(props.driverVersion, deviceDesc.vendor);
                deviceDesc.apiName = fmt::format("Vulkan {}.{}.{}",
                                                VK_VERSION_MAJOR(props.apiVersion),
                                                VK_VERSION_MINOR(props.apiVersion),
                                                VK_VERSION_PATCH(props.apiVersion));
            }
        }
    }

    if (bestDevice == VK_NULL_HANDLE || bestQueueIndex == ~0u)
    {
        LOG(Error, "Failed to find GPU with Graphics/Compute support!");
        return false;
    }

    physicalDevice = bestDevice;
    graphicsQueueIndex = bestQueueIndex;

    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    if (deviceProperties.limits.timestampPeriod == 0.0f)
    {
        LOG(Error, "Timestamp queries not supported on this GPU");
        return false;
    }

    // Querying device extensions
    u32 deviceExtensionCount;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr));
    Vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount,
                                                  deviceExtensions.data()));

    if (!ValidateVulkanProperties(requiredDeviceExtensions, deviceExtensions, &VkExtensionProperties::extensionName))
    {
        LOG(Error, "Required device extensions are missing!!!");
        return false;
    }

    VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR fifoLatestReadyFeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR,
        .pNext = nullptr
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

    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features12
    };

#ifdef USE_DESCRIPTOR_BUFFER
    VkPhysicalDeviceDescriptorBufferFeaturesEXT featuresDescBuf{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = &features13
    };

    VkPhysicalDeviceFeatures2 supportedCore{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &featuresDescBuf
    };
#else
    VkPhysicalDeviceFeatures2 supportedCore{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features13
    };
#endif

    vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedCore);


    // storing this for fallback logic later
    this->useUnifiedLayout = (unifiedLayoutFeature.unifiedImageLayouts == VK_TRUE);
    if (this->useUnifiedLayout)
    {
        unifiedLayoutFeature.unifiedImageLayouts = VK_TRUE;
        LOG(Info, "Vulkan Unified Image Layouts enabled.");
    }
    else
    {
        // fallback
        unifiedLayoutFeature.unifiedImageLayouts = VK_FALSE;
    }

    // Optional Features!!
    if (fifoLatestReadyFeature.presentModeFifoLatestReady)
    {
        LOG(Info, "FIFO_LATEST_READY is supported and enabled.");
        fifoLatestReadyFeature.presentModeFifoLatestReady = VK_TRUE;
    }
    else
    {
        LOG(Warning, "FIFO_LATEST_READY not supported. Falling back to standard FIFO.");
        fifoLatestReadyFeature.presentModeFifoLatestReady = VK_FALSE;
    }


    // check for the things that matter for now
    if (!features13.dynamicRendering || !features13.synchronization2 || !features12.bufferDeviceAddress)
    {
        LOG(Error, "Device missing mandatory Vulkan 1.2/1.3 features!");
        return false;
    }

#ifdef USE_DESCRIPTOR_BUFFER
    if (!featuresDescBuf.descriptorBuffer)
    {
        LOG(Error, "Mandatory Extension Feature Missing: Descriptor Buffer ");
    }
#endif

    // slang...
    features11.shaderDrawParameters = VK_TRUE;
    // 1.2 Features
    features12.descriptorIndexing = VK_TRUE;
    features12.scalarBlockLayout = VK_TRUE;
    features12.hostQueryReset = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;

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

    // Initialize immediate submitter
    immediateSubmitter.Init(this);

    if (!this->useUnifiedLayout)
    {
        LOG(Warning, "Unified Image Layouts not supported. Falling back to legacy transitions.");
    }
    return true;
}

void VulkanDevice::ImmediateSubmit(const std::function<void(GPUCommandBuffer*)> func)
{
    immediateSubmitter.Submit([&](VkCommandBuffer vkCmd) {
        VulkanCommandBuffer wrapper;
        wrapper.InitFromHandle(this, vkCmd);
        func(&wrapper);
    }, "RHI Immediate");
}

std::unique_ptr<GPUTexture> VulkanDevice::CreateTexture(TextureInfo& info)
{
    return std::make_unique<VulkanTexture>(this, info);
}

std::unique_ptr<GPUSampler> VulkanDevice::CreateSampler(SamplerInfo& info)
{
    return std::make_unique<VulkanSampler>(this, info);
}

std::unique_ptr<GPUBuffer> VulkanDevice::CreateBuffer(BufferInfo& info)
{
    return std::make_unique<VulkanBuffer>(this, info);
}

std::unique_ptr<GPUShader> VulkanDevice::CreateShader(std::span<const u32> code)
{
    return std::make_unique<VulkanShader>(this, code);
}

std::unique_ptr<GPUShader> VulkanDevice::CreateShaderPath(const char* path)
{
    auto code = FileManager::LoadSPV(path);

    if (!code) {
        LOG(Error, "Failed to create shader from path: {}", path);
        return nullptr;
    }

    return CreateShader(*code);
}

void ImmediateSubmitter::Init(VulkanDevice* device)
{
    this->device = device;

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device->graphicsQueueIndex
    };
    VK_CHECK(vkCreateCommandPool(device->device, &poolInfo, nullptr, &immCommandPool));

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = immCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(device->device, &allocInfo, &immCommandBuffer));

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0
    };
    VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &immFence));
}

void ImmediateSubmitter::Destroy()
{
    if (device && device->device)
    {
        if (immFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device->device, immFence, nullptr);
            immFence = VK_NULL_HANDLE;
        }
        if (immCommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device->device, immCommandPool, nullptr);
            immCommandPool = VK_NULL_HANDLE;
        }
    }
    immCommandBuffer = VK_NULL_HANDLE;
    device = nullptr;
}

void VulkanDevice::Destroy()
{
    immediateSubmitter.Destroy();

    if (allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }

    if (device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    physicalDevice = nullptr;
    graphicsQueue = nullptr;
    instance = nullptr;
}