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

#ifdef _WIN32
// Disable secure CRT warnings since we prefer to use portable CRT functions
#define _CRT_SECURE_NO_WARNINGS
// Reduce the amount of stuff pulled in by windows.h
#define WIN32_LEAN_AND_MEAN
// Avoid bad interactions with std::min, std::max
#define NOMINMAX
#include <windows.h>
#endif // _WIN32

// We use function pointers for everything, so disable the prototypes
#define VK_NO_PROTOTYPES

#include "vulkan/vulkan.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>


static void PrintMessage(const char *msg)
{
    printf("%s", msg);
#ifdef _WIN32
    // It's awkward to read stdout in Visual Studio, so duplicate the message
    // into VS's debug output window
    OutputDebugStringA(msg);
#endif
}

static void PrintfMessage(const char *fmt, ...)
{
    const size_t MAX_MESSAGE_SIZE = 1024;
    char buf[MAX_MESSAGE_SIZE];

    va_list ap;
    va_start(ap, fmt);
#ifdef _WIN32
    _vsnprintf(buf, MAX_MESSAGE_SIZE, fmt, ap);
#else
    vsnprintf(buf, MAX_MESSAGE_SIZE, fmt, ap);
#endif
    buf[MAX_MESSAGE_SIZE - 1] = '\0';
    va_end(ap);
    PrintMessage(buf);
}

// Trivial logging system
#define LOGI(fmt, ...) do { PrintfMessage("[INFO] " fmt "\n", __VA_ARGS__); } while (0)
#define LOGW(fmt, ...) do { PrintfMessage("[WARN] " fmt "\n", __VA_ARGS__); } while (0)
#define LOGE(fmt, ...) do { PrintfMessage("[ERROR] " fmt "\n", __VA_ARGS__); } while (0)

#define ASSERT(cond) do { if (!(cond)) { LOGE("Assertion failed: %s:%d: %s", __FILE__, __LINE__, #cond); abort(); } } while (0)

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
    X(vkCreateDevice) \
    X(vkDestroyDevice)

struct InstanceFunctions
{
#define X(n) PFN_##n n;
    INSTANCE_FUNCTIONS
#undef X
};

class VksxsInstance
{
    VkInstance m_Instance;
    PFN_vkDestroyInstance m_vkDestroyInstance;
public:
    VksxsInstance(VkInstance instance, const InstanceFunctions &pfn)
        : m_Instance(instance), m_vkDestroyInstance(pfn.vkDestroyInstance) { }
    ~VksxsInstance() { m_vkDestroyInstance(m_Instance, nullptr); }
    operator VkInstance() { return m_Instance; }

    // Prevent copying
    VksxsInstance(const VksxsInstance &) = delete;
    VksxsInstance &operator=(const VksxsInstance &) = delete;
};

static bool RunDemo()
{
    auto pfn_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)LoadGlobalSymbol("vkGetInstanceProcAddr");
    if (!pfn_vkGetInstanceProcAddr)
    {
        LOGE("Failed to find vkGetInstanceProcAddr - maybe you don't have any Vulkan drivers installed");
        return false;
    }

    auto pfn_vkCreateInstance = (PFN_vkCreateInstance)pfn_vkGetInstanceProcAddr(nullptr, "vkCreateInstance");
    if (!pfn_vkCreateInstance)
    {
        LOGE("Failed to find vkCreateInstance");
        return false;
    }

    VkResult result;

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

    VkInstance unwrappedInstance;
    result = pfn_vkCreateInstance(&instanceCreateInfo, nullptr, &unwrappedInstance);
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateInstance failed (%d)", result);
        return false;
    }

    // Try loading the per-instance functions
    InstanceFunctions pfn = {};
    bool ok = true;
#define X(n) \
        pfn.n = (PFN_##n)pfn_vkGetInstanceProcAddr(unwrappedInstance, #n); \
        if (!pfn.n) { \
            LOGE("Failed to get instance symbol %s", #n); \
            ok = false; \
        }
    INSTANCE_FUNCTIONS
#undef X

    // Abort if we didn't manage to load all the symbols we want
    if (!ok)
    {
        // Try to clean up, but be careful because we might not have even
        // got the vkDestroyInstance function - in that case we have no safe
        // choice but to leak the instance
        if (pfn.vkDestroyInstance)
            pfn.vkDestroyInstance(unwrappedInstance, nullptr);
        return false;
    }

    // Set up a RAII wrapper so we don't need to worry about calling
    // vkDestroyInstance manually
    VksxsInstance instance(unwrappedInstance, pfn);

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
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    VkDevice device;
    result = pfn.vkCreateDevice(preferredPhysicalDevice, &deviceCreateInfo, nullptr, &device);
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateDevice failed (%d)", result);
        return false;
    }

    LOGI("Successfully created device %p", device);

    pfn.vkDestroyDevice(device, nullptr);

    return true;
}

int main()
{
    if (!RunDemo())
        return -1;
    return 0;
}
