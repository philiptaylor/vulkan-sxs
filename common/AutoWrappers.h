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

#ifndef INCLUDED_VKSXS_AUTO_WRAPPERS
#define INCLUDED_VKSXS_AUTO_WRAPPERS

#include "common/Common.h"

#include "common/AllocationCallbacks.h"
#include "common/InstanceFunctions.h"

template <typename T, typename FN, FN InstanceFunctions::*CB>
class WrapDispatchable
{
    T m_Handle;
    FN m_vkDestroy;
public:
    WrapDispatchable()
        : m_Handle(VK_NULL_HANDLE), m_vkDestroy(nullptr)
    {
    }

    WrapDispatchable(const InstanceFunctions &pfn, T handle = VK_NULL_HANDLE)
        : m_Handle(handle), m_vkDestroy(pfn.*CB)
    {
        ASSERT(m_vkDestroy != nullptr);
    }

    ~WrapDispatchable()
    {
        ASSERT(!(m_Handle && !m_vkDestroy));
        if (m_vkDestroy)
            m_vkDestroy(m_Handle, CREATE_ALLOCATOR());
    }

    operator T() { return m_Handle; }
    T *ptr() { return &m_Handle; }

    // Allow moving but not copying

    WrapDispatchable(const WrapDispatchable &) = delete;
    WrapDispatchable &operator=(const WrapDispatchable &) = delete;

    WrapDispatchable(WrapDispatchable &&v)
        : m_Handle(v.m_Handle), m_vkDestroy(v.m_vkDestroy)
    {
        v.m_Handle = VK_NULL_HANDLE;
        v.m_vkDestroy = nullptr;
    }

    WrapDispatchable &operator=(WrapDispatchable &&v)
    {
        if (this != &v)
        {
            ASSERT(!(m_Handle && !m_vkDestroy));
            if (m_vkDestroy)
                m_vkDestroy(m_Handle, CREATE_ALLOCATOR());
            m_Handle = v.m_Handle;
            m_vkDestroy = v.m_vkDestroy;
            v.m_Handle = VK_NULL_HANDLE;
            v.m_vkDestroy = nullptr;
        }
        return *this;
    }
};

template <typename P, typename T, typename FN, FN InstanceFunctions::*CB>
class WrapNonDispatchable
{
    P m_Parent;
    T m_Handle;
    FN m_vkDestroy;
public:
    WrapNonDispatchable()
        : m_Parent(VK_NULL_HANDLE), m_Handle(VK_NULL_HANDLE), m_vkDestroy(nullptr)
    {
    }

    WrapNonDispatchable(const InstanceFunctions &pfn, P parent, T handle = VK_NULL_HANDLE)
        : m_Parent(parent), m_Handle(handle), m_vkDestroy(pfn.*CB)
    {
    }

    ~WrapNonDispatchable()
    {
        ASSERT(!(m_Handle && !m_vkDestroy));
        if (m_vkDestroy)
            m_vkDestroy(m_Parent, m_Handle, CREATE_ALLOCATOR());
    }

    operator T() { return m_Handle; }
    T *ptr() { return &m_Handle; }

    // Allow moving but not copying

    WrapNonDispatchable(const WrapNonDispatchable &) = delete;
    WrapNonDispatchable &operator=(const WrapNonDispatchable &) = delete;

    WrapNonDispatchable(WrapNonDispatchable &&v)
        : m_Parent(v.m_Parent), m_Handle(v.m_Handle), m_vkDestroy(v.m_vkDestroy)
    {
        v.m_Parent = VK_NULL_HANDLE;
        v.m_Handle = VK_NULL_HANDLE;
        v.m_vkDestroy = nullptr;
    }

    WrapNonDispatchable &operator=(WrapNonDispatchable &&v)
    {
        if (this != &v)
        {
            ASSERT(!(m_Handle && !m_vkDestroy));
            if (m_vkDestroy)
                m_vkDestroy(m_Parent, m_Handle, CREATE_ALLOCATOR());
            m_Parent = v.m_Parent;
            m_Handle = v.m_Handle;
            m_vkDestroy = v.m_vkDestroy;
            v.m_Parent = VK_NULL_HANDLE;
            v.m_Handle = VK_NULL_HANDLE;
            v.m_vkDestroy = nullptr;
        }
        return *this;
    }
};

typedef WrapDispatchable<VkInstance, PFN_vkDestroyInstance, &InstanceFunctions::vkDestroyInstance> AutoVkInstance;
typedef WrapDispatchable<VkDevice, PFN_vkDestroyDevice, &InstanceFunctions::vkDestroyDevice> AutoVkDevice;

typedef WrapNonDispatchable<VkInstance, VkDebugReportCallbackEXT, PFN_vkDestroyDebugReportCallbackEXT, &InstanceFunctions::vkDestroyDebugReportCallbackEXT> AutoVkDebugReportCallbackEXT;
typedef WrapNonDispatchable<VkDevice, VkCommandPool, PFN_vkDestroyCommandPool, &InstanceFunctions::vkDestroyCommandPool> AutoVkCommandPool;
typedef WrapNonDispatchable<VkDevice, VkImage, PFN_vkDestroyImage, &InstanceFunctions::vkDestroyImage> AutoVkImage;
typedef WrapNonDispatchable<VkDevice, VkDeviceMemory, PFN_vkFreeMemory, &InstanceFunctions::vkFreeMemory> AutoVkDeviceMemory;
typedef WrapNonDispatchable<VkDevice, VkSemaphore, PFN_vkDestroySemaphore, &InstanceFunctions::vkDestroySemaphore> AutoVkSemaphore;

#endif // INCLUDED_VKSXS_AUTO_WRAPPERS
