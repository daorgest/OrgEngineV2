//
// Created by Orgest on 6/10/2025.
//
#include "VulkanInit.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "VulkanCheck.h"
#include "Tools/Logger.h"
#include "Tools/Vector.h"
#include "tracy/Tracy.hpp"

using namespace Renderer;

static Vector requiredExtensions = {
	VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
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

static Vector requiredDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
	VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
	VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};


#ifdef VULKAN_DEBUG_MODE
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* pUserData = nullptr)
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

bool ValidateExtensions(const Vector<const char*>& required, const Vector<VkExtensionProperties>& available)
{
	bool allFound = true;

	for (const char* name : required)
	{
		bool found = false;
		for (const auto& [extensionName, specVersion] : available)
		{
			if (strcmp(extensionName, name) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			LOG(Error,"Missing required Vulkan Extension/Layer: {}", name);
			allFound = false;
		}
	}

	return allFound;
}

bool VulkanInstance::Init()
{
	ZoneScopedN("Init Vulkan Instance");
	LOG(Info, "Init Vulkan Instance");
	VK_CHECK(volkInitialize());

	// Checking for instance extensions!!
	u32 extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	Vector<VkExtensionProperties> available(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, available.data());

	ValidateExtensions(requiredExtensions, available);

	appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "OrgEngine",
		.applicationVersion = 1,
		.engineVersion = 1,
		.apiVersion = VK_HEADER_VERSION_COMPLETE
	};

	// comparing with the ones I want
	Vector<const char*> enabledExtensions = requiredExtensions;
	for (const auto& ext : requiredExtensions) {
		enabledExtensions.push_back(ext);
	}

#ifdef VULKAN_DEBUG_MODE
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = DebugCallback,
	};

	u32 layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	Vector<VkLayerProperties> layers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

	// For Validating layer properties that I want
	bool foundLayer = false;
	for (const char* req : kValidationLayers)
	{
		for (const auto& layer : layers)
		{
			if (strcmp(layer.layerName, req) == 0)
			{
				foundLayer = true;
				break;
			}
		}
	}

	if (!foundLayer)
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

void VulkanInstance::Destroy() const
{
	if (instance != nullptr)
	{
#ifdef VULKAN_DEBUG_MODE
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
		vkDestroyInstance(instance, nullptr);
	}
}

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
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   score += 2000;
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 200;
		score += vram / (128ull * 1024ull * 1024);  // +1 per 128 MB VRAM
		score += props.limits.maxImageDimension2D / 1024;
		score += (props.driverVersion & 0xFFF);        // small tie-breaker

		if (score > bestScore)
		{
			bestScore      = score;
			bestDevice     = dev;
			bestQueueIndex = gfxComputeIndex;

			// fill deviceDesc for UI
			deviceDesc.name                 = props.deviceName;
			deviceDesc.vendor               = static_cast<GPUVendor>(props.vendorID);
			deviceDesc.type                 = static_cast<GPUDeviceType>(props.deviceType);
			deviceDesc.driverVersion        = props.driverVersion;
			deviceDesc.dedicatedVideoMemory = vram;
		}
	}

	if (bestDevice == VK_NULL_HANDLE || bestQueueIndex == ~0u)
	{
		LOG(Error, "Failed to find GPU with Graphics/Compute support!");
		return false;
	}

	physicalDevice     = bestDevice;
	graphicsQueueIndex = bestQueueIndex;



	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	if (deviceProperties.limits.timestampPeriod == 0.0f) {
		LOG(Error, "Timestamp queries not supported on this GPU");
		return false;
	}

	// Querying device extensions
	u32 deviceExtensionCount;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr));
	Vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, deviceExtensions.data()));

	if (!ValidateExtensions(requiredDeviceExtensions, deviceExtensions))
	{
		LOG(Error, "Required device extensions are missing!!!");
		return false;
	}

	// Check optional FIFO_LATEST_READY
	bool supportsFifoLatestReady = false;
	for (const auto& [extensionName, specVersion] : deviceExtensions) {
		if (strcmp(extensionName, "VK_KHR_present_mode_fifo_latest_ready") == 0 ||
			strcmp(extensionName, "VK_EXT_present_mode_fifo_latest_ready") == 0)
		{
			LOG(Info, "Optional extension supported: {}", extensionName);
			requiredDeviceExtensions.push_back(extensionName);
			supportsFifoLatestReady = true;
			break;
		}
	}


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

	if (supportsFifoLatestReady && fifoLatestReadyFeature.presentModeFifoLatestReady) {
		LOG(Info, "FIFO_LATEST_READY feature is supported");
		fifoLatestReadyFeature.presentModeFifoLatestReady = VK_TRUE;
	} else {
		LOG(Warning, "FIFO_LATEST_READY feature not supported - will fallback.");
		fifoLatestReadyFeature.presentModeFifoLatestReady = VK_FALSE;
	}

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

	InitImmediate();
	return true;
}

bool VulkanDevice::InitImmediate()
{
	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = graphicsQueueIndex
	};
	VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &immCommandPool));

	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = immCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &immCommandBuffer));

	VkFenceCreateInfo fenceInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = 0
	};
	VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &immFence));

	return true;
}

std::string VulkanDevice::DecodeDriverVersion(u32 driverVersion, GPUVendor vendor)
{
	switch (vendor)
	{
	case GPUVendor::Nvidia:
		return std::to_string((driverVersion >> 22) & 0x3FF) + "." +
			   std::to_string((driverVersion >> 14) & 0xFF);

	case GPUVendor::Intel:
		return std::to_string(driverVersion >> 14) + "." +
			   std::to_string(driverVersion & 0x3FFF);

	case GPUVendor::AMD:
		return std::to_string((driverVersion >> 22) & 0x3FF) + "." +
			   std::to_string((driverVersion >> 12) & 0x3FF) + "." +
			   std::to_string(driverVersion & 0xFFF);

	case GPUVendor::Apple: // Apple tends to report plain values, no decode
		return std::to_string(driverVersion);

	default: // Unknown Vendor
		return "Unknown Driver Version (0x" + fmt::format("{:X}", driverVersion) + ")";
	}
}


void VulkanDevice::Destroy()
{
	// Destroying immediate Commands first
	vkDestroyFence(device, immFence, nullptr);
	vkDestroyCommandPool(device, immCommandPool, nullptr);

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
