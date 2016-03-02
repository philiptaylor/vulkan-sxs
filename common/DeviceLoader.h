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

#ifndef INCLUDED_VKSXS_DEVICE_LOADER
#define INCLUDED_VKSXS_DEVICE_LOADER

#define ENABLE_DEBUG_ALLOCATOR 1
#define ENABLE_DEBUG_REPORT_VERBOSE 0

#include "common/Common.h"

#include "common/AutoWrappers.h"
#include "common/InstanceFunctions.h"

class DeviceLoader
{
public:
    DeviceLoader();

    ~DeviceLoader();

    void SetEnableApiDump(bool enable) { m_EnableApiDump = enable; }
    void SetDebugReportFlags(VkDebugReportFlagsEXT flags) { m_DebugReportFlags = flags; }

    bool Setup();

    const InstanceFunctions &GetInstanceFunctions() const { return m_InstanceFunctions; }

    VkPhysicalDevice GetPhysicalDevice() { return m_PhysicalDevice; }
    VkDevice GetDevice() { return m_Device; }

    uint32_t GetGraphicsQueueFamily() { return m_GraphicsQueueFamily; }
    uint32_t GetTransferQueueFamily() { return m_TransferQueueFamily; }
    VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
    VkQueue GetTransferQueue() { return m_TransferQueue; }

private:
    bool m_EnableApiDump;
    VkDebugReportFlagsEXT m_DebugReportFlags;

    AutoVkInstance m_Instance;
    AutoVkDebugReportCallbackEXT m_DebugReportCallback;
    AutoVkDevice m_Device;

    VkPhysicalDevice m_PhysicalDevice;

    uint32_t m_GraphicsQueueFamily;
    uint32_t m_TransferQueueFamily;
    VkQueue m_GraphicsQueue;
    VkQueue m_TransferQueue;

    InstanceFunctions m_InstanceFunctions;
};

#endif // INCLUDED_VKSXS_DEVICE_LOADER
