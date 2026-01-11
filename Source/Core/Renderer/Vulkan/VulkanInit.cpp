//
// Created by Orgest on 6/10/2025.
//
#include "VulkanInit.h"
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



#ifdef VULKAN_DEBUG_MODE
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                    void* pUserData = nullptr)
{
    LogType type = LogType::Debug;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        type = LogType::Error;
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        type = LogType::Warning;
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        type = LogType::Info;

    // Simplify Type String
    auto typeStr = "GENERAL";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        typeStr = "VALIDATION";
    else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        typeStr = "PERFORMANCE";

    Logger::Write(type, std::source_location::current(), "[Vulkan] [{}] ({}): {}",
                typeStr,
                callbackData->pMessageIdName ? callbackData->pMessageIdName : "None",
                callbackData->pMessage ? callbackData->pMessage : "No message");
    return VK_FALSE;
}
#endif

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