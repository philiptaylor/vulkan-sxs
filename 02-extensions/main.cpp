/*
 * Copyright (c) 2016 Philip Taylor
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define ENABLE_DEBUG_ALLOCATOR 1
#define ENABLE_DEBUG_REPORT_VERBOSE 0
#define ENABLE_APIDUMP 0

#include "common/Common.h"

#include "common/AllocationCallbacks.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

static void *LoadGlobalSymbol(const char *symbol)
{
#ifdef _WIN32
    HMODULE module = LoadLibraryA("vulkan-1.dll");
    if (!module)
        return nullptr;
    return GetProcAddress(module, symbol);
#else
    void *handle = dlopen("libvulkan.so", RTLD_LOCAL|RTLD_LAZY);
    if (!handle)
        return nullptr;
    return dlsym(handle, symbol);
#endif
    // Don't bother calling FreeLibrary()/dlclose() because we don't mind
    // leaving the library loaded until the process terminates
}

static std::string VersionToString(uint32_t version)
{
    const size_t BUF_LEN = 32;
    char buf[BUF_LEN];
    snprintf(buf, BUF_LEN, "%d.%d.%d", (version >> 22) & 0x3ff, (version >> 12) & 0x3ff, version & 0xfff);
    return std::string(buf);
}


#define INSTANCE_FUNCTIONS \
    X(vkDestroyInstance) \
    X(vkEnumeratePhysicalDevices) \
    X(vkGetPhysicalDeviceProperties) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) \
    X(vkEnumerateDeviceLayerProperties) \
    X(vkEnumerateDeviceExtensionProperties) \
    X(vkCreateDevice) \
    X(vkDestroyDevice)

#define INSTANCE_FUNCTIONS_EXT_DEBUG_REPORT \
    X(vkCreateDebugReportCallbackEXT) \
    X(vkDestroyDebugReportCallbackEXT)

struct InstanceFunctions
{
#define X(n) PFN_##n n;
    INSTANCE_FUNCTIONS
    INSTANCE_FUNCTIONS_EXT_DEBUG_REPORT
#undef X
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char *pLayerPrefix,
    const char *pMessage,
    void *pUserData)
{
    const char *severity;
    // Multiple bits might be set, so pick the most severe one
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        severity = "ERROR";
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        severity = "WARN";
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        severity = "PERF";
    else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        severity = "INFO";
    else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
        severity = "DBUG"; // intentional misspelling to make the widths prettier
    else
        severity = "???";

    if (ENABLE_DEBUG_REPORT_VERBOSE)
        PrintfMessage("[%s][CALLBACK] %s: %s [flags=0x%x objectType=%d object=0x%" PRIx64 " location=%d messageCode=%d pUserData=%p]\n",
            severity, pLayerPrefix, pMessage, flags, objectType, object, location, messageCode, pUserData);
    else
        PrintfMessage("[%s][CALLBACK] %s: %s\n", severity, pLayerPrefix, pMessage);

    return VK_FALSE;
}

class VksxsInstance
{
    VkInstance m_Instance;
    PFN_vkDestroyInstance m_vkDestroyInstance;
public:
    VksxsInstance(const InstanceFunctions &pfn, VkInstance instance)
        : m_Instance(instance), m_vkDestroyInstance(pfn.vkDestroyInstance) { }
    ~VksxsInstance() { m_vkDestroyInstance(m_Instance, CREATE_ALLOCATOR()); }
    operator VkInstance() { return m_Instance; }

    // Prevent copying
    VksxsInstance(const VksxsInstance &) = delete;
    VksxsInstance &operator=(const VksxsInstance &) = delete;
};

class VksxsDebugReportCallbackEXT
{
    VkInstance m_Instance;
    VkDebugReportCallbackEXT m_Handle;
    PFN_vkDestroyDebugReportCallbackEXT m_vkDestroyDebugReportCallback;
public:
    VksxsDebugReportCallbackEXT(const InstanceFunctions &pfn, VkInstance instance, VkDebugReportCallbackEXT handle = VK_NULL_HANDLE)
        : m_Handle(handle), m_Instance(instance), m_vkDestroyDebugReportCallback(pfn.vkDestroyDebugReportCallbackEXT) { }
    ~VksxsDebugReportCallbackEXT() { m_vkDestroyDebugReportCallback(m_Instance, m_Handle, CREATE_ALLOCATOR()); }
    operator VkDebugReportCallbackEXT() { return m_Handle; }

    VkDebugReportCallbackEXT *ptr() { return &m_Handle; }

    // Prevent copying
    VksxsDebugReportCallbackEXT(const VksxsDebugReportCallbackEXT &) = delete;
    VksxsDebugReportCallbackEXT &operator=(const VksxsDebugReportCallbackEXT &) = delete;
};

static bool RunDemo()
{
    AllocationCallbacksBase::test();

    if (ENABLE_APIDUMP)
    {
        // The validation layers read some settings from vk_layer_settings.txt
        // (in the current working directory), and there appears to be no API
        // to set them programmatically. I prefer to configure this kind of stuff
        // through the application itself, instead of relying on the environment,
        // so generate vk_layer_settings.txt ourselves.

        std::ofstream settings_file("vk_layer_settings.txt");

        settings_file << "# GENERATED FILE - DO NOT EDIT\n";

        // Setting names for the 1.0.3 SDK
        settings_file << "ApiDumpDetailed = TRUE\n";
        settings_file << "ApiDumpNoAddr = FALSE\n";
        settings_file << "ApiDumpFile = TRUE\n";
        settings_file << "ApiDumpLogFilename = vk_apidump.txt\n";
        settings_file << "ApiDumpFlush = FALSE\n";

        // Setting names for slightly newer versions of the SDK
        settings_file << "lunarg_api_dump.detailed = TRUE\n";
        settings_file << "lunarg_api_dump.no_addr = FALSE\n";
        settings_file << "lunarg_api_dump.file = TRUE\n";
        settings_file << "lunarg_api_dump.log_filename = vk_apidump.txt\n";
        settings_file << "lunarg_api_dump.flush = FALSE\n";
    }

    auto pfn_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)LoadGlobalSymbol("vkGetInstanceProcAddr");
    if (!pfn_vkGetInstanceProcAddr)
    {
        LOGE("Failed to find vkGetInstanceProcAddr - maybe you don't have any Vulkan drivers installed");
        return false;
    }

#define X(n) \
        auto pfn_##n = (PFN_##n)pfn_vkGetInstanceProcAddr(nullptr, #n); \
        if (!pfn_vkCreateInstance) { \
            LOGE("Failed to find %s", #n); \
            return false; \
        }
    X(vkCreateInstance);
    X(vkEnumerateInstanceExtensionProperties)
    X(vkEnumerateInstanceLayerProperties)
#undef X

    VkResult result;

    uint32_t instanceLayerCount;
    result = pfn_vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
    if (result != VK_SUCCESS)
    {
        LOGE("vkEnumerateInstanceLayerProperties failed (%d)", result);
        return false;
    }
    LOGI("%d instance layers", instanceLayerCount);

    std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
    if (instanceLayerCount > 0)
    {
        result = pfn_vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data());
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateInstanceLayerProperties failed (%d)", result);
            return false;
        }
    }

    for (auto layer : instanceLayers)
    {
        LOGI("Instance layer: \"%s\", spec version %s, impl version %d, \"%s\"",
            layer.layerName,
            VersionToString(layer.specVersion).c_str(),
            layer.implementationVersion,
            layer.description);

        uint32_t instanceLayerExtensionCount;
        result = pfn_vkEnumerateInstanceExtensionProperties(layer.layerName, &instanceLayerExtensionCount, nullptr);
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateInstanceExtensionProperties failed (%d)", result);
            return false;
        }

        std::vector<VkExtensionProperties> instanceLayerExtensions(instanceLayerExtensionCount);
        if (instanceLayerExtensionCount > 0)
        {
            result = pfn_vkEnumerateInstanceExtensionProperties(layer.layerName, &instanceLayerExtensionCount, instanceLayerExtensions.data());
            if (result != VK_SUCCESS)
            {
                LOGE("vkEnumerateInstanceExtensionProperties failed (%d)", result);
                return false;
            }
        }

        for (auto extension : instanceLayerExtensions)
        {
            LOGI("    Instance layer extension: \"%s\", spec version %d",
                extension.extensionName, extension.specVersion);
        }
    }

    uint32_t instanceExtensionCount;
    result = pfn_vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
    if (result != VK_SUCCESS)
    {
        LOGE("vkEnumerateInstanceExtensionProperties failed (%d)", result);
        return false;
    }
    LOGI("%d instance extensions", instanceExtensionCount);

    std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
    if (instanceLayerCount > 0)
    {
        result = pfn_vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data());
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateInstanceExtensionProperties failed (%d)", result);
            return false;
        }
    }

    for (auto extension : instanceExtensions)
    {
        LOGI("Instance extension: \"%s\", spec version %d",
            extension.extensionName, extension.specVersion);
    }

    std::vector<std::string> desiredInstanceLayers;
    std::vector<std::string> desiredInstanceExtensions;
    std::vector<std::string> desiredDeviceLayers;
    std::vector<std::string> desiredDeviceExtensions;

    if (ENABLE_APIDUMP)
    {
        desiredInstanceLayers.push_back("VK_LAYER_LUNARG_api_dump");
        desiredDeviceLayers.push_back("VK_LAYER_LUNARG_api_dump");
    }

    desiredInstanceLayers.insert(desiredInstanceLayers.end(), {
        "VK_LAYER_LUNARG_threading",
        "VK_LAYER_LUNARG_param_checker",
        "VK_LAYER_LUNARG_device_limits",
        "VK_LAYER_LUNARG_object_tracker",
        "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_mem_tracker",
        "VK_LAYER_LUNARG_draw_state",
        "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_GOOGLE_unique_objects",
    });

    desiredInstanceExtensions.insert(desiredInstanceExtensions.end(), {
        "VK_EXT_debug_report",
    });

    desiredDeviceLayers.insert(desiredDeviceLayers.end(), {
        "VK_LAYER_LUNARG_threading",
        "VK_LAYER_LUNARG_param_checker",
        "VK_LAYER_LUNARG_device_limits",
        "VK_LAYER_LUNARG_object_tracker",
        "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_mem_tracker",
        "VK_LAYER_LUNARG_draw_state",
        "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_GOOGLE_unique_objects",
    });


    // Map from layer name to set of extension names
    std::map<std::string, std::set<std::string>> instanceAvailableLayers;
    for (auto layer : instanceLayers)
    {
        std::set<std::string> extensionNames;

        uint32_t instanceLayerExtensionCount;
        result = pfn_vkEnumerateInstanceExtensionProperties(layer.layerName, &instanceLayerExtensionCount, nullptr);
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateInstanceExtensionProperties failed (%d)", result);
            return false;
        }

        std::vector<VkExtensionProperties> instanceLayerExtensions(instanceLayerExtensionCount);
        if (instanceLayerExtensionCount > 0)
        {
            result = pfn_vkEnumerateInstanceExtensionProperties(layer.layerName, &instanceLayerExtensionCount, instanceLayerExtensions.data());
            if (result != VK_SUCCESS)
            {
                LOGE("vkEnumerateInstanceExtensionProperties failed (%d)", result);
                return false;
            }
        }

        for (auto extension : instanceLayerExtensions)
            extensionNames.insert(extension.extensionName);

        instanceAvailableLayers[layer.layerName] = std::move(extensionNames);
    }

    std::vector<const char *> instanceEnabledLayerNames;
    std::vector<const char *> instanceEnabledExtensionNames;
    // NOTE: These contain char* pointers to instanceAvailable{Layers,Extensions}.c_str()
    // so be careful not to trigger any reallocation in those Available objects
    // until we've called vkCreateInstance and finished using the char*s

    std::set<std::string> instanceAvailableExtensions;

    for (auto name : desiredInstanceLayers)
    {
        auto it = instanceAvailableLayers.find(name);
        if (it != instanceAvailableLayers.end())
        {
            LOGI("Enabling instance layer %s", it->first.c_str());
            instanceEnabledLayerNames.push_back(it->first.c_str());

            instanceAvailableExtensions.insert(it->second.begin(), it->second.end());
        }
        else
        {
            LOGI("Cannot find desired instance layer %s", name.c_str());
        }
    }

    for (auto name : desiredInstanceExtensions)
    {
        auto it = instanceAvailableExtensions.find(name);
        if (it != instanceAvailableExtensions.end())
        {
            LOGI("Enabling instance extension %s", it->c_str());
            instanceEnabledExtensionNames.push_back(it->c_str());
        }
        else
        {
            LOGI("Cannot find desired instance extension %s", name.c_str());
        }
    }

    std::set<std::string> instanceEnabledLayers(instanceEnabledLayerNames.begin(), instanceEnabledLayerNames.end());
    std::set<std::string> instanceEnabledExtensions(instanceEnabledExtensionNames.begin(), instanceEnabledExtensionNames.end());

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "vksxs";
    applicationInfo.applicationVersion = 1;
    applicationInfo.pEngineName = "vksxs";
    applicationInfo.engineVersion = 1;
    applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 3);

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = (uint32_t)instanceEnabledLayerNames.size();
    instanceCreateInfo.ppEnabledLayerNames = instanceEnabledLayerNames.data();
    instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceEnabledExtensionNames.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceEnabledExtensionNames.data();

    VkDebugReportCallbackCreateInfoEXT debugReportCreateInfo = {};
    debugReportCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debugReportCreateInfo.flags |= VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
    debugReportCreateInfo.flags |= VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debugReportCreateInfo.flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debugReportCreateInfo.flags |= VK_DEBUG_REPORT_ERROR_BIT_EXT;
    debugReportCreateInfo.flags |= VK_DEBUG_REPORT_DEBUG_BIT_EXT;
    debugReportCreateInfo.pfnCallback = debugReportCallbackCallback;
    instanceCreateInfo.pNext = &debugReportCreateInfo;

    VkInstance unwrappedInstance;
    result = pfn_vkCreateInstance(&instanceCreateInfo, CREATE_ALLOCATOR(), &unwrappedInstance);
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateInstance failed (%d)", result);
        return false;
    }

    // Try loading the per-instance functions
    bool ok = true;
    InstanceFunctions pfn = {};
#define X(n) \
        pfn.n = (PFN_##n)pfn_vkGetInstanceProcAddr(unwrappedInstance, #n); \
        if (!pfn.n) { \
            LOGE("Failed to get instance symbol %s", #n); \
            ok = false; \
        }

    INSTANCE_FUNCTIONS
    if (instanceEnabledExtensions.count("VK_EXT_debug_report"))
    {
        INSTANCE_FUNCTIONS_EXT_DEBUG_REPORT
    }
#undef X

    // Abort if we didn't manage to load all the symbols we want
    if (!ok)
    {
        // Try to clean up, but be careful because we might not have even
        // got the vkDestroyInstance function - in that case we have no safe
        // choice but to leak the instance
        if (pfn.vkDestroyInstance)
            pfn.vkDestroyInstance(unwrappedInstance, CREATE_ALLOCATOR());
        return false;
    }

    // Set up a RAII wrapper so we don't need to worry about calling
    // vkDestroyInstance manually
    VksxsInstance instance(pfn, unwrappedInstance);

    VksxsDebugReportCallbackEXT debugReportCallback(pfn, instance);
    if (instanceEnabledExtensions.count("VK_EXT_debug_report"))
    {
        result = pfn.vkCreateDebugReportCallbackEXT(instance, &debugReportCreateInfo, CREATE_ALLOCATOR(), debugReportCallback.ptr());
        if (result != VK_SUCCESS)
        {
            LOGE("vkCreateDebugReportCallbackEXT failed (%d)", result);
            return false;
        }
    }

    // Now we've got the instance, so we can find the physical devices

    uint32_t physicalDeviceCount;
    result = pfn.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    if (result != VK_SUCCESS)
    {
        LOGE("vkEnumeratePhysicalDevices failed (%d)", result);
        return false;
    }

    if (physicalDeviceCount == 0)
    {
        LOGE("No physical devices found - maybe you don't have any Vulkan drivers installed");
        return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = pfn.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    if (result != VK_SUCCESS)
    {
        LOGE("vkEnumeratePhysicalDevices failed (%d)", result);
        return false;
    }

    for (auto physicalDevice : physicalDevices)
    {
        VkPhysicalDeviceProperties properties;
        pfn.vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        // Dump some basic information.
        // (driverVersion doesn't have to be packed in format defined by Vulkan,
        // but it might be, so we'll decode and print it in that form in case it's helpful.)
        LOGI("Device: \"%s\", API version %s, driver version %d (%s), vendor 0x%04x, device 0x%04x, type %d",
            properties.deviceName, VersionToString(properties.apiVersion).c_str(),
            properties.driverVersion, VersionToString(properties.driverVersion).c_str(),
            properties.vendorID, properties.deviceID,
            properties.deviceType);
    }

    // Use the hopelessly inadequate approach of choosing the first from the list
    VkPhysicalDevice preferredPhysicalDevice = physicalDevices[0];



    uint32_t deviceLayerCount;
    result = pfn.vkEnumerateDeviceLayerProperties(preferredPhysicalDevice, &deviceLayerCount, nullptr);
    if (result != VK_SUCCESS)
    {
        LOGE("vkEnumerateDeviceLayerProperties failed (%d)", result);
        return false;
    }
    LOGI("%d device layers", deviceLayerCount);

    std::vector<VkLayerProperties> deviceLayers(deviceLayerCount);
    if (deviceLayerCount > 0)
    {
        result = pfn.vkEnumerateDeviceLayerProperties(preferredPhysicalDevice, &deviceLayerCount, deviceLayers.data());
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateDeviceLayerProperties failed (%d)", result);
            return false;
        }
    }

    for (auto layer : deviceLayers)
    {
        LOGI("Device layer: \"%s\", spec version %s, impl version %d, \"%s\"",
            layer.layerName,
            VersionToString(layer.specVersion).c_str(),
            layer.implementationVersion,
            layer.description);

        uint32_t deviceLayerExtensionCount;
        result = pfn.vkEnumerateDeviceExtensionProperties(preferredPhysicalDevice, layer.layerName, &deviceLayerExtensionCount, nullptr);
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateDeviceExtensionProperties failed (%d)", result);
            return false;
        }

        std::vector<VkExtensionProperties> deviceLayerExtensions(deviceLayerExtensionCount);
        if (deviceLayerExtensionCount > 0)
        {
            result = pfn.vkEnumerateDeviceExtensionProperties(preferredPhysicalDevice, layer.layerName, &deviceLayerExtensionCount, deviceLayerExtensions.data());
            if (result != VK_SUCCESS)
            {
                LOGE("vkEnumerateDeviceExtensionProperties failed (%d)", result);
                return false;
            }
        }

        for (auto extension : deviceLayerExtensions)
        {
            LOGI("    Device layer extension: \"%s\", spec version %d",
                extension.extensionName, extension.specVersion);
        }
    }

    uint32_t deviceExtensionCount;
    result = pfn.vkEnumerateDeviceExtensionProperties(preferredPhysicalDevice, nullptr, &deviceExtensionCount, nullptr);
    if (result != VK_SUCCESS)
    {
        LOGE("vkEnumerateDeviceExtensionProperties failed (%d)", result);
        return false;
    }
    LOGI("%d device extensions", deviceExtensionCount);

    std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
    if (deviceLayerCount > 0)
    {
        result = pfn.vkEnumerateDeviceExtensionProperties(preferredPhysicalDevice, nullptr, &deviceExtensionCount, deviceExtensions.data());
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateDeviceExtensionProperties failed (%d)", result);
            return false;
        }
    }

    for (auto extension : deviceExtensions)
    {
        LOGI("Device extension: \"%s\", spec version %d",
            extension.extensionName, extension.specVersion);
    }

    // Map from layer name to set of extension names
    std::map<std::string, std::set<std::string>> deviceAvailableLayers;
    for (auto layer : deviceLayers)
    {
        std::set<std::string> extensionNames;

        uint32_t deviceLayerExtensionCount;
        result = pfn.vkEnumerateDeviceExtensionProperties(preferredPhysicalDevice, layer.layerName, &deviceLayerExtensionCount, nullptr);
        if (result != VK_SUCCESS)
        {
            LOGE("vkEnumerateDeviceExtensionProperties failed (%d)", result);
            return false;
        }

        std::vector<VkExtensionProperties> deviceLayerExtensions(deviceLayerExtensionCount);
        if (deviceLayerExtensionCount > 0)
        {
            result = pfn.vkEnumerateDeviceExtensionProperties(preferredPhysicalDevice, layer.layerName, &deviceLayerExtensionCount, deviceLayerExtensions.data());
            if (result != VK_SUCCESS)
            {
                LOGE("vkEnumerateDeviceExtensionProperties failed (%d)", result);
                return false;
            }
        }

        for (auto extension : deviceLayerExtensions)
            extensionNames.insert(extension.extensionName);

        deviceAvailableLayers[layer.layerName] = std::move(extensionNames);
    }

    std::vector<const char *> deviceEnabledLayerNames;
    std::vector<const char *> deviceEnabledExtensionNames;
    // NOTE: These contain char* pointers to deviceAvailable{Layers,Extensions}.c_str()
    // so be careful not to trigger any reallocation in those Available objects
    // until we've called vkCreateDevice and finished using the char*s

    std::set<std::string> deviceAvailableExtensions;

    for (auto name : desiredDeviceLayers)
    {
        auto it = deviceAvailableLayers.find(name);
        if (it != deviceAvailableLayers.end())
        {
            LOGI("Enabling device layer %s", it->first.c_str());
            deviceEnabledLayerNames.push_back(it->first.c_str());

            deviceAvailableExtensions.insert(it->second.begin(), it->second.end());
        }
        else
        {
            LOGI("Cannot find desired device layer %s", name.c_str());
        }
    }

    for (auto name : desiredDeviceExtensions)
    {
        auto it = deviceAvailableExtensions.find(name);
        if (it != deviceAvailableExtensions.end())
        {
            LOGI("Enabling device extension %s", it->c_str());
            deviceEnabledExtensionNames.push_back(it->c_str());
        }
        else
        {
            LOGI("Cannot find desired device extension %s", name.c_str());
        }
    }



    VkPhysicalDeviceProperties preferredPhysicalDeviceProperties;
    pfn.vkGetPhysicalDeviceProperties(preferredPhysicalDevice, &preferredPhysicalDeviceProperties);

    uint32_t queueFamilyPropertyCount;
    pfn.vkGetPhysicalDeviceQueueFamilyProperties(preferredPhysicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    pfn.vkGetPhysicalDeviceQueueFamilyProperties(preferredPhysicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    for (auto family : queueFamilyProperties)
    {
        LOGI("Queue family: flags 0x%08x, count %u, timestampValidBits %u, minImageTransferGranularity (%u,%u,%u)",
            family.queueFlags, family.queueCount, family.timestampValidBits,
            family.minImageTransferGranularity.width,
            family.minImageTransferGranularity.height,
            family.minImageTransferGranularity.depth);
    }

    int graphicsQueueFamilyIdx = -1;
    int transferQueueFamilyIdx = -1;
    for (size_t i = 0; i < queueFamilyProperties.size(); ++i)
    {
        if (graphicsQueueFamilyIdx == -1 && (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            graphicsQueueFamilyIdx = (int)i;

        if (transferQueueFamilyIdx == -1 && (queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT))
            transferQueueFamilyIdx = (int)i;
    }

    ASSERT(graphicsQueueFamilyIdx != -1);
    ASSERT(transferQueueFamilyIdx != -1);

    VkPhysicalDeviceFeatures enabledFeatures = {};

    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;

    float defaultPriority = 1.0f;

    {
        VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
        deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIdx;
        deviceQueueCreateInfo.queueCount = 1;
        deviceQueueCreateInfo.pQueuePriorities = &defaultPriority;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    if (transferQueueFamilyIdx != graphicsQueueFamilyIdx)
    {
        VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
        deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfo.queueFamilyIndex = transferQueueFamilyIdx;
        deviceQueueCreateInfo.queueCount = 1;
        deviceQueueCreateInfo.pQueuePriorities = &defaultPriority;
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = (uint32_t)deviceQueueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = (uint32_t)deviceEnabledLayerNames.size();
    deviceCreateInfo.ppEnabledLayerNames = deviceEnabledLayerNames.data();
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceEnabledExtensionNames.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceEnabledExtensionNames.data();
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    VkDevice device;
    result = pfn.vkCreateDevice(preferredPhysicalDevice, &deviceCreateInfo, CREATE_ALLOCATOR(), &device);
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateDevice failed (%d)", result);
        return false;
    }

    LOGI("Successfully created device %p", device);

    pfn.vkDestroyDevice(device, CREATE_ALLOCATOR());

    return true;
}

int main()
{
    if (!RunDemo())
        return -1;
    return 0;
}
