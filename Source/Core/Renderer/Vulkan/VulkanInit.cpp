//
// Created by Orgest on 6/10/2025.
//
#include "VulkanInit.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "VulkanCheck.h"

#if ENGINE_PLATFORM_SDL
#include "SDL3/SDL_vulkan.h"
#endif
#include "Tools/Array.h"
#include "Tools/Logger.h"
#include "Tools/Vector.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

static Vector requiredExtensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef ENGINE_PLATFORM_WIN32
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef VULKAN_DEBUG_MODE
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

#ifdef VULKAN_DEBUG_MODE
static Vector kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#endif

static Array requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME
};


#ifdef VULKAN_DEBUG_MODE
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                    void* pUserData = nullptr)
{
    const char* severityStr = "UNKNOWN";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severityStr = "ERROR";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severityStr = "WARNING";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        severityStr = "INFO";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        severityStr = "VERBOSE";

    auto typeStr = "GENERAL";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        typeStr = "VALIDATION";
    else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        typeStr = "PERFORMANCE";

    const char* message = callbackData->pMessage ? callbackData->pMessage : "No message";


    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LOG(Error, "[Vulkan] [{} | {}] ({}): {}", severityStr, typeStr, callbackData->pMessageIdName, message);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LOG(Warning, "[Vulkan] [{} | {}] ({}): {}", severityStr, typeStr, callbackData->pMessageIdName, message);
    }
    else
    {
        LOG(Debug, "[Vulkan] [{} | {}] ({}): {}", severityStr, typeStr, callbackData->pMessageIdName, message);
    }

    return VK_FALSE; // don't stop Vulkan calls
}
#endif

template <typename T>
bool ValidateVulkanProperties(std::span<const char*> required, const Vector<T>& available,
                              const char (T::*nameField)[VK_MAX_EXTENSION_NAME_SIZE])
{
    bool ok = true;

    for (const char* req : required)
    {
        bool found = false;

        for (const auto& prop : available)
        {
            if (strcmp(prop.*nameField, req) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG(Error, "Missing required Vulkan property: {}", req);
            ok = false;
        }
    }

    return ok;
}

bool VulkanInstance::Init()
{
    ZoneScopedN("Init Vulkan Instance");
    LOG(Info, "Init Vulkan Instance");
    VK_CHECK(volkInitialize());

    Vector<const char*> enabledExtensions;

    enabledExtensions.reserve(requiredExtensions.size() + 8);
    for (const char* ext : requiredExtensions)
        enabledExtensions.push_back(ext);

#if ENGINE_PLATFORM_SDL
    {
        LOG(Info, "[SDL3] Querying Vulkan instance extensions");

        Uint32 extCount = 0;
        const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);

        if (!sdlExts || extCount == 0)
        {
            LOG(Error, "[SDL3] SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
            return false;
        }

        for (Uint32 i = 0; i < extCount; i++)
        {
            const char* ext = sdlExts[i];
            LOG(Debug, "[SDL3] Adding Vulkan instance extension: {}", ext);
            enabledExtensions.push_back(ext);
        }
    }
#endif


    // Checking for instance extensions!!
    u32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    Vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, available.data());

    ValidateVulkanProperties(requiredExtensions, available, &VkExtensionProperties::extensionName);

    appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "OrgEngine",
        .applicationVersion = 1,
        .engineVersion = 1,
        .apiVersion = VK_HEADER_VERSION_COMPLETE
    };

#ifdef VULKAN_DEBUG_MODE
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback,
    };

    u32 layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    Vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    if (!ValidateVulkanProperties(kValidationLayers, layers, &VkLayerProperties::layerName))
    {
        LOG(Error, "Validation Layer '{}' not found.\n"
            "You are running a Vulkan debug build which requires the Vulkan SDK "
            "and validation layers to be installed.\n"
            "Download from: https://vulkan.lunarg.com/sdk/home",
            kValidationLayers[0]);
        return false;
    }
#endif

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#ifdef VULKAN_DEBUG_MODE
        .pNext = &debugCreateInfo,
#else
        .pNext = nullptr,
#endif
        .flags = 0,
        .pApplicationInfo = &appInfo,
#ifdef VULKAN_DEBUG_MODE
        .enabledLayerCount = static_cast<u32>(kValidationLayers.size()),
        .ppEnabledLayerNames = kValidationLayers.data(),
#else
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
#endif
        .enabledExtensionCount = static_cast<u32>(enabledExtensions.size()),
        .ppEnabledExtensionNames = enabledExtensions.data(),
    };

    // Create Vulkan Instance
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

    // Volk!!
    volkLoadInstance(instance);
#ifdef VULKAN_DEBUG_MODE
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger));
#endif
    LOG(Info, "Vulkan Instance created successfully");

    return true;
}

void VulkanInstance::Destroy()
{
    if (instance != nullptr)
    {
#ifdef VULKAN_DEBUG_MODE
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
        vkDestroyInstance(instance, nullptr);
    }
}

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
    ZoneScopedN("Init Device");
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

        int gfxComputeIndex = -1;
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

        // calculate VRAM
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(dev, &mp);
        u64 vram = 0;
        for (u32 h = 0; h < mp.memoryHeapCount; ++h)
        {
            if (mp.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                vram += mp.memoryHeaps[h].size;
        }

        // scoring system to pick best gpu for the job
        u64 score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 2000;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 200;
        score += vram / (128ull * 1024ull * 1024); // +1 per 128 MB VRAM
        score += props.limits.maxImageDimension2D / 1024;
        score += (props.driverVersion & 0xFFF); // small tie-breaker

        if (score > bestScore)
        {
            bestScore = score;
            bestDevice = dev;
            bestQueueIndex = gfxComputeIndex;

            // fill deviceDesc for UI
            deviceDesc.name = props.deviceName;
            deviceDesc.vendor = static_cast<GPUVendor>(props.vendorID);
            deviceDesc.type = static_cast<GPUDeviceType>(props.deviceType);
            deviceDesc.driverVersion = props.driverVersion;
            deviceDesc.dedicatedVideoMemory = vram;
            deviceDesc.driverVersionString = DecodeDriverVersion(props.driverVersion, deviceDesc.vendor);
            deviceDesc.apiName = fmt::format("Vulkan {}.{}.{}", VK_VERSION_MAJOR(props.apiVersion),
                                             VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
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

    // Check optional FIFO_LATEST_READY
    // bool supportsFifoLatestReady = false;
    // for (const auto& [extensionName, specVersion] : deviceExtensions) {
    // 	if (strcmp(extensionName, "VK_KHR_present_mode_fifo_latest_ready") == 0 ||
    // 		strcmp(extensionName, "VK_EXT_present_mode_fifo_latest_ready") == 0)
    // 	{
    // 		LOG(Info, "Optional extension supported: {}", extensionName);
    // 		requiredDeviceExtensions.push_back(extensionName);
    // 		supportsFifoLatestReady = true;
    // 		break;
    // 	}
    // }


    VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR fifoLatestReadyFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR,
        .pNext = nullptr
    };

    VkPhysicalDeviceVulkan11Features features11{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &fifoLatestReadyFeature
    };

    VkPhysicalDeviceVulkan12Features features12{
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

    // check for the things that matter for now
    if (!features13.dynamicRendering)
    {
        LOG(Error, "Device does not support dynamic rendering!");
        return false;
    }
    if (!features13.synchronization2)
    {
        LOG(Error, "Device does not support Synchronization2 feature!");
        return false;
    }
    if (!features12.bufferDeviceAddress)
    {
        LOG(Error, "Device does not support buffer device address!");
        return false;
    }

    // slang...
    features11.shaderDrawParameters = VK_TRUE;
    // 1.2 Features
    features12.descriptorIndexing = VK_TRUE;
    features12.scalarBlockLayout = VK_TRUE;
    features12.hostQueryReset = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;

    // 1.3 Features
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    // if (supportsFifoLatestReady && fifoLatestReadyFeature.presentModeFifoLatestReady) {
    // 	LOG(Info, "FIFO_LATEST_READY feature is supported");
    // 	fifoLatestReadyFeature.presentModeFifoLatestReady = VK_TRUE;
    // } else {
    // 	LOG(Warning, "FIFO_LATEST_READY feature not supported - will fall back.");
    // 	fifoLatestReadyFeature.presentModeFifoLatestReady = VK_FALSE;
    // }

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
    return true;
}

void ImmediateSubmitter::Init(VulkanDevice* dev)
{
    this->device = dev;

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
    if (device&& device
    
    ->
    device
    )
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

std::string VulkanDevice::DecodeDriverVersion(u32 driverVersion, const GPUVendor vendor)
{
    switch (vendor)
    {
    case GPUVendor::Nvidia:
        return fmt::format("{}.{}", (driverVersion >> 22) & 0x3FF, (driverVersion >> 14) & 0xFF);

    case GPUVendor::Intel:
        return fmt::format("{}.{}", driverVersion >> 14, driverVersion & 0x3FFF);

    case GPUVendor::AMD:
        return fmt::format("{}.{}.{}", (driverVersion >> 22) & 0x3FF, (driverVersion >> 12) & 0x3FF,
                           driverVersion & 0xFFF);

    case GPUVendor::Apple:
        // Apple tends to report plain numeric driver version
        return fmt::format("{}", driverVersion);

    default:
        return fmt::format("Unknown Driver (0x{:X})", driverVersion);
    }
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