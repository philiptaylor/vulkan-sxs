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

#ifndef INCLUDED_VKSXS_ALLOCATION_CALLBACKS
#define INCLUDED_VKSXS_ALLOCATION_CALLBACKS

#include "common/Log.h"

/*
 * Implements the (non-trivial) VkAllocationCallbacks semantics on top of the
 * system's standard malloc implementation. This is useful when you just want
 * to add a layer of logging around all allocations, and don't need to change
 * the allocation behaviour itself.
 */
class AllocationCallbacksBase
{
public:
    static void *doAllocation(
        size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

    static void *doReallocation(void *pOriginal,
        size_t size, size_t alignment, VkSystemAllocationScope allocationScope,
        size_t *originalSize);

    static void doFree(void *pMemory, size_t *originalSize);

    // Convert enums into strings for logging
    static const char *scopeString(VkSystemAllocationScope scope);
    static const char *typeString(VkInternalAllocationType type);

    // Run some basic sanity tests
    static void test();
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
 * or through a macro:
 *   vkFoo(..., CREATE_ALLOCATOR());
 */
class DebugAllocationCallbacks : private AllocationCallbacksBase
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

    static VKAPI_ATTR void* VKAPI_CALL fnAllocation(void *pUserData,
        size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

    static VKAPI_ATTR void* VKAPI_CALL fnReallocation(void *pUserData,
        void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);

    static VKAPI_ATTR void VKAPI_CALL fnFree(void *pUserData, void *pMemory);

    static VKAPI_ATTR void VKAPI_CALL fnInternalAllocation(void *pUserData,
        size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);

    static VKAPI_ATTR void VKAPI_CALL fnInternalFree(void *pUserData,
        size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
};

#if ENABLE_DEBUG_ALLOCATOR
#define ALLOCATOR_SOURCE_STRINGIFY(line) #line
#define ALLOCATOR_SOURCE_STRING(file, line) file ":" ALLOCATOR_SOURCE_STRINGIFY(line)
#define CREATE_ALLOCATOR() (&DebugAllocationCallbacks::createCallbacks(ALLOCATOR_SOURCE_STRING(__FILE__, __LINE__)))
#else
#define CREATE_ALLOCATOR() (nullptr)
#endif

#endif // INCLUDED_VKSXS_ALLOCATION_CALLBACKS
