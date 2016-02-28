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

#ifdef _WIN32
// Disable secure CRT warnings since we prefer to use portable CRT functions
# define _CRT_SECURE_NO_WARNINGS
// Use WIN32_LEAN_AND_MEAN to reduce the amount of stuff pulled in by windows.h
# define WIN32_LEAN_AND_MEAN
// Avoid interactions with std::min, std::max
# define NOMINMAX

# include <windows.h>
#endif

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

/*
 * Provider of VkAllocationCallbacks, which simply logs every operation.
 *
 * If you are passing it directly into a Vulkan API call, you can construct a
 * temporary VkAllocationCallbacks and pass its pointer into the API - the
 * temporary won't be destroyed until after the API call has returned.
 *
 * i.e. you can use it like:
 *   vkFoo(..., &DebugAllocator::createCallbacks("some identifier"));
 */
class DebugAllocator
{
public:
    /*
     * src should be a global static string, and must at least remain valid
     * until the end of the Vulkan scope that used these callbacks.
     * This will be printed with each allocation, to help you tell where
     * they came from.
     */
    static VkAllocationCallbacks createCallbacks(const char *src)
    {
        VkAllocationCallbacks callbacks = {};
        callbacks.pUserData = (void *)src;
        callbacks.pfnAllocation = fnAllocation;
        callbacks.pfnReallocation = fnReallocation;
        callbacks.pfnFree = fnFree;
        callbacks.pfnInternalAllocation = fnInternalAllocation;
        callbacks.pfnInternalFree = fnInternalFree;
        return callbacks;
    }

private:
    static const char *scopeString(VkSystemAllocationScope scope)
    {
        switch (scope) {
        case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND: return "command";
        case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT: return "object";
        case VK_SYSTEM_ALLOCATION_SCOPE_CACHE: return "cache";
        case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE: return "device";
        case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE: return "instance";
        default: return "???";
        }
    }

    static const char *typeString(VkInternalAllocationType type)
    {
        switch (type) {
        case VK_INTERNAL_ALLOCATION_TYPE_EXECUTABLE: return "executable";
        default: return "???";
        }
    }

    /*
     * Aligned allocation with support for realloc is a big pain on Linux.
     *
     * aligned_alloc() can provide alignment, but there's no aligned_realloc().
     * realloc() can do reallocation, but can't provide alignment.
     *
     * We could always allocate a new aligned buffer and free the old one -
     * except that we need to know the size of the old one, so we can copy
     * its contents into the new one, and we can't know that unless we track it
     * in our own std::map<void*, size_t> or equivalent.
     *
     * So the approach we use here is:
     *
     * Allocations are done with malloc()/realloc()/free().
     * We add enough padding onto the requested size, so that we can
     * allocate unaligned then round up to the requested alignment without
     * overflowing the buffer.
     *
     * We also store a BufferHeader structure just before the
     * aligned buffer, which tells us the size of the allocation and of
     * the padding, so that we can realloc/free it correctly later.
     *
     * So the malloced data looks like:
     *
     *  .---------.--------------.----------------.---------.
     *  | padding | BufferHeader | requested size | padding |
     *  '---------'--------------'----------------'---------'
     *  ^                        ^
     *  |                        |
     *  outer                    inner (with requested alignment)
     *
     * where each 'padding' is zero or more bytes.
     */

    struct BufferHeader
    {
        void *outer; // unaligned pointer returned by malloc()
        size_t size; // original size requested in the allocation call
    };

    static VKAPI_ATTR void* VKAPI_CALL fnAllocation(void *pUserData,
        size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
    {
        const char *src = (const char *)pUserData;
        LOGI("alloc: %s: size=%lu alignment=%lu allocationScope=%s",
            src, (unsigned long)size, (unsigned long)alignment, scopeString(allocationScope));

        return doAllocation(size, alignment);
    }

    static void *doAllocation(size_t size, size_t alignment)
    {
        // The spec requires a return value of NULL when size is 0,
        // so handle that case explicitly
        if (size == 0)
            return nullptr;

        // Allocate enough space for the padding and BufferHeader
        void *outer = malloc(alignment + sizeof(BufferHeader) + size);
        if (!outer)
            return nullptr;

        // Add enough space for BufferHeader, then round up to alignment
        uintptr_t inner = ((uintptr_t)outer + sizeof(BufferHeader) + alignment - 1) & ~(alignment - 1);

        BufferHeader header;
        header.outer = outer;
        header.size = size;

        memcpy((void *)(inner - sizeof(BufferHeader)), &header, sizeof(BufferHeader));

        LOGI("    (allocated with outer=%p inner=%p)", outer, (void *)inner);

        return (void *)inner;
    }

    static VKAPI_ATTR void* VKAPI_CALL fnReallocation(void *pUserData,
        void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
    {
        const char *src = (const char *)pUserData;
        LOGI("realloc: %s: pOriginal=%p size=%lu alignment=%lu allocationScope=%s",
            src, pOriginal, (unsigned long)size, (unsigned long)alignment, scopeString(allocationScope));

        if (pOriginal == 0)
            return doAllocation(size, alignment);

        if (size == 0)
        {
            doFree(pOriginal);
            return nullptr;
        }

        uintptr_t inner = (uintptr_t)pOriginal;

        BufferHeader header;
        memcpy(&header, (void *)(inner - sizeof(BufferHeader)), sizeof(BufferHeader));

        LOGI("    (original size=%lu)", (unsigned long)header.size);

        // First, have a go at simply calling realloc() and hope that
        // maybe it's still aligned correctly

        void *newOuter = realloc(header.outer, alignment + sizeof(BufferHeader) + size);
        if (!newOuter)
            return nullptr;

        // Check whether the inner buffer is still aligned okay
        // (which is true iff the outer buffer has moved by a multiple of
        // the alignment, including when that multiple is zero)

        if ((((uintptr_t)newOuter - (uintptr_t)header.outer) & (alignment - 1)) == 0)
        {
            // Good, we can still use this buffer. realloc() already copied
            // the inner contents, we just need to update the header.

            // Add enough space for BufferHeader, then round up to alignment
            uintptr_t newInner = ((uintptr_t)newOuter + sizeof(BufferHeader) + alignment - 1) & ~(alignment - 1);

            header.outer = newOuter;
            header.size = size;

            memcpy((void *)(newInner - sizeof(BufferHeader)), &header, sizeof(BufferHeader));

            LOGI("    (reallocated with outer=%p inner=%p)", newOuter, (void *)newInner);

            return (void *)newInner;
        }
        else
        {
            // Oh dear, realloc() broke the alignment. Let's abort that attempt,
            // then allocate a new buffer and copy across.

            // Find the (misaligned) realloced inner buffer
            uintptr_t badInner = (uintptr_t)newOuter + (inner - (uintptr_t)header.outer);

            // Get a totally new aligned buffer
            void *newInner = doAllocation(size, alignment);
            if (!newInner)
                return nullptr;

            // Copy the inner buffer
            memcpy(newInner, (void *)badInner, std::min(size, header.size));

            // Release the bad realloc()
            free(newOuter);

            LOGI("    (reallocated slowly with outer=%p inner=%p)", newOuter, (void *)newInner);

            return newInner;
        }
    }

    static VKAPI_ATTR void VKAPI_CALL fnFree(void *pUserData, void *pMemory)
    {
        const char *src = (const char *)pUserData;
        LOGI("free: %s: pMemory=%p", src, pMemory);

        doFree(pMemory);
    }

    static void doFree(void *pMemory)
    {
        uintptr_t inner = (uintptr_t)pMemory;

        BufferHeader header;
        memcpy(&header, (void *)(inner - sizeof(BufferHeader)), sizeof(BufferHeader));

        LOGI("    (original size=%lu, outer=%p)", (unsigned long)header.size, header.outer);

        free(header.outer);
    }

    static VKAPI_ATTR void VKAPI_CALL fnInternalAllocation(void *pUserData,
        size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
    {
        const char *src = (const char *)pUserData;
        LOGI("internal allocation: %s: size=%lu type=%s allocationScope=%s",
            src, typeString(allocationType), scopeString(allocationScope));
    }

    static VKAPI_ATTR void VKAPI_CALL fnInternalFree(void *pUserData,
        size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
    {
        const char *src = (const char *)pUserData;
        LOGI("internal free: %s: size=%lu type=%s allocationScope=%s",
            src, typeString(allocationType), scopeString(allocationScope));
    }
};

#if ENABLE_DEBUG_ALLOCATOR
#define ALLOCATOR_SOURCE_STRINGIFY(line) #line
#define ALLOCATOR_SOURCE_STRING(file, line) file ":" ALLOCATOR_SOURCE_STRINGIFY(line)
#define CREATE_ALLOCATOR() (&DebugAllocator::createCallbacks(ALLOCATOR_SOURCE_STRING(__FILE__, __LINE__)))
#else
#define CREATE_ALLOCATOR() (nullptr)
#endif

int main()
{
    PrintMessage("Hello world\n");

    auto pfn_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)LoadGlobalSymbol("vkGetInstanceProcAddr");
    if (!pfn_vkGetInstanceProcAddr)
    {
        LOGE("Failed to find vkGetInstanceProcAddr - maybe you don't have any Vulkan drivers installed.");
        return false;
    }
    LOGI("%p", pfn_vkGetInstanceProcAddr);

    auto pfn_vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)pfn_vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceExtensionProperties");
    auto pfn_vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)pfn_vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties");
    auto pfn_vkCreateInstance = (PFN_vkCreateInstance)pfn_vkGetInstanceProcAddr(nullptr, "vkCreateInstance");

    LOGI("%p %p %p", pfn_vkEnumerateInstanceExtensionProperties, pfn_vkEnumerateInstanceLayerProperties, pfn_vkCreateInstance);

    VkResult result;

    uint32_t instanceLayerCount;
    result = pfn_vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
    ASSERT(result == VK_SUCCESS);
    LOGI("%d instance layers", instanceLayerCount);

    std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
    if (instanceLayerCount > 0)
    {
        result = pfn_vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data());
        ASSERT(result == VK_SUCCESS);
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
        ASSERT(result == VK_SUCCESS);

        std::vector<VkExtensionProperties> instanceLayerExtensions(instanceLayerExtensionCount);
        if (instanceLayerExtensionCount > 0)
        {
            result = pfn_vkEnumerateInstanceExtensionProperties(layer.layerName, &instanceLayerExtensionCount, instanceLayerExtensions.data());
            ASSERT(result == VK_SUCCESS);
        }

        for (auto extension : instanceLayerExtensions)
        {
            LOGI("    Instance layer extension: \"%s\", spec version %d",
                extension.extensionName, extension.specVersion);
        }
    }

    uint32_t instanceExtensionCount;
    result = pfn_vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
    ASSERT(result == VK_SUCCESS);
    LOGI("%d instance extensions", instanceExtensionCount);

    std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
    if (instanceLayerCount > 0)
    {
        result = pfn_vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data());
        ASSERT(result == VK_SUCCESS);
    }

    for (auto extension : instanceExtensions)
    {
        LOGI("Instance extension: \"%s\", spec version %d",
            extension.extensionName, extension.specVersion);
    }

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "vksxs";
    applicationInfo.apiVersion = VK_MAKE_VERSION(1, 0, 3);

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;

    VkInstance instance;
    result = pfn_vkCreateInstance(&instanceCreateInfo, CREATE_ALLOCATOR(), &instance);
    ASSERT(result == VK_SUCCESS);

    // Try loading the per-instance functions
    bool ok = true;
    InstanceFunctions pfn = {};
#define X(n) \
    do { \
        pfn.n = (PFN_##n)pfn_vkGetInstanceProcAddr(instance, #n); \
        if (!pfn.n) { \
            LOGE("Failed to get instance symbol %s", #n); \
            ok = false; \
        } \
    } while (0);
    INSTANCE_FUNCTIONS
#undef X

    // Abort if we didn't manage to load all the symbols we want
    if (!ok)
    {
        // Try to clean up, but be careful because we might not have even
        // got the vkDestroyInstance function - in that case we have no safe
        // choice but to leak the instance
        if (pfn.vkDestroyInstance)
            pfn.vkDestroyInstance(instance, CREATE_ALLOCATOR());

        return false;
    }

    // Now we've got the instance, so we can find the physical devices

    uint32_t physicalDeviceCount;
    result = pfn.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    ASSERT(result == VK_SUCCESS);

    if (physicalDeviceCount == 0)
    {
        LOGE("No physical devices found - maybe you don't have any Vulkan drivers installed.");
        pfn.vkDestroyInstance(instance, CREATE_ALLOCATOR()); // TODO: RAII cleanup?
        return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = pfn.vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    ASSERT(result == VK_SUCCESS);

    for (auto physicalDevice : physicalDevices)
    {
        VkPhysicalDeviceProperties properties;
        pfn.vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        // Dump some basic information.
        // (driverVersion doesn't have to be packed in format defined by Vulkan,
        // but it might be, so we'll decode and print it in that form in case it's helpful.)
        LOGI("Device: \"%s\", API version %s, driver version %s (%d), vendor 0x%04x, device 0x%04x, type %d",
            properties.deviceName, VersionToString(properties.apiVersion).c_str(),
            VersionToString(properties.driverVersion).c_str(),
            properties.driverVersion,
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
    deviceCreateInfo.queueCreateInfoCount = deviceQueueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    VkDevice device;
    result = pfn.vkCreateDevice(preferredPhysicalDevice, &deviceCreateInfo, CREATE_ALLOCATOR(), &device);
    ASSERT(result == VK_SUCCESS);

    pfn.vkDestroyDevice(device, CREATE_ALLOCATOR());

    pfn.vkDestroyInstance(instance, CREATE_ALLOCATOR());
}
