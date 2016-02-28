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

#include "common/Common.h"

#include "common/AllocationCallbacks.h"

#include <algorithm>
#include <cstdlib>

const char *AllocationCallbacksBase::scopeString(VkSystemAllocationScope scope)
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

const char *AllocationCallbacksBase::typeString(VkInternalAllocationType type)
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
  * its contents into the new one, and we wouldn't know that unless we track it
  * in our own std::map<void*, size_t> or equivalent.
  *
  * So the approach we use here is:
  *
  * Allocations are done with malloc()/realloc()/free(). We add enough padding
  * onto the requested size so that we can allocate unaligned then round up to
  * the requested alignment without overflowing the buffer.
  *
  * We also store a BufferHeader structure just before the aligned buffer,
  * which tells us the size of the allocation and of the padding, so that we
  * can realloc/free it correctly later.
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
  *
  * (On Windows we could just use _aligned_malloc, _aligned_realloc,
  * _aligned_free; but they are unlikely to provide much performance benefit,
  * and there's less chance of platform-specific bugs if we use the same code
  * on all platforms.)
  */

struct BufferHeader
{
    void *outer; // unaligned pointer returned by malloc()
    size_t size; // original size requested in the allocation call
};

void *AllocationCallbacksBase::doAllocation(size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    // Must be a power of two
    ASSERT(alignment != 0 && !(alignment & (alignment - 1)));

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

    // Store the header just before inner
    memcpy((void *)(inner - sizeof(BufferHeader)), &header, sizeof(BufferHeader));

    return (void *)inner;
}

void *AllocationCallbacksBase::doReallocation(void *pOriginal,
    size_t size, size_t alignment, VkSystemAllocationScope allocationScope,
    size_t *originalSize)
{
    // Must be a power of two
    ASSERT(alignment != 0 && !(alignment & (alignment - 1)));

    *originalSize = 0;

    if (pOriginal == nullptr)
        return doAllocation(size, alignment, allocationScope);

    if (size == 0)
    {
        doFree(pOriginal, originalSize);
        return nullptr;
    }

    uintptr_t inner = (uintptr_t)pOriginal;

    // Read the header from the original allocation
    BufferHeader header;
    memcpy(&header, (void *)(inner - sizeof(BufferHeader)), sizeof(BufferHeader));

    *originalSize = header.size;

    // If we can be certain that realloc will return a correctly-aligned
    // pointer (which typically means alignment <= alignof(double)) then it's
    // most efficient to simply use that.
    //
    // Otherwise, we have no choice but to allocate a fresh buffer and copy
    // the data across. (We can't speculatively try a realloc and hope that it
    // just shrinks the buffer and preserves alignment - the problem is that
    // if realloc breaks the alignment, and we need to fall back to the
    // fresh-buffer-and-copy method, but the fresh allocation fails, we will
    // have already freed the original buffer (in realloc). We can only legally
    // return NULL if we guarantee the original buffer is still valid.)

#ifdef _WIN32
    static const size_t minReallocAlignment = __alignof(double);
#else
    static const size_t minReallocAlignment = alignof(double);
#endif

    if (alignment <= minReallocAlignment)
    {
        void *newOuter = realloc(header.outer, alignment + sizeof(BufferHeader) + size);
        if (!newOuter)
            return nullptr;

        // Verify realloc returned the alignment we expected
        ASSERT(((uintptr_t)newOuter & (alignment - 1)) == 0);

        // realloc() already copied the inner contents, we just need to update the header.

        // Add enough space for BufferHeader, then round up to alignment
        uintptr_t newInner = ((uintptr_t)newOuter + sizeof(BufferHeader) + alignment - 1) & ~(alignment - 1);

        // Double-check our calculations
        ASSERT(newInner - inner == (uintptr_t)newOuter - (uintptr_t)header.outer);

        header.outer = newOuter;
        header.size = size;

        // Store the updated header
        memcpy((void *)(newInner - sizeof(BufferHeader)), &header, sizeof(BufferHeader));

        return (void *)newInner;
    }
    else
    {
        // Get a totally new aligned buffer
        void *newInner = doAllocation(size, alignment, allocationScope);
        if (!newInner)
            return nullptr;

        // Copy the inner buffer
        memcpy(newInner, (void *)inner, std::min(size, header.size));

        // Release the original buffer
        free(header.outer);

        return newInner;
    }
}

void AllocationCallbacksBase::doFree(void *pMemory, size_t *originalSize)
{
    uintptr_t inner = (uintptr_t)pMemory;

    BufferHeader header;
    memcpy(&header, (void *)(inner - sizeof(BufferHeader)), sizeof(BufferHeader));

    *originalSize = header.size;

    free(header.outer);
}

void AllocationCallbacksBase::test()
{
    VkSystemAllocationScope scope = VK_SYSTEM_ALLOCATION_SCOPE_COMMAND;
    size_t originalSize;

    ASSERT(doAllocation(0, 1, scope) == 0);

    {
        size_t size = 1;
        for (size_t align = 1; align <= 65536; align *= 2)
        {
            void *b = doAllocation(size, align, scope);
            ASSERT(b != nullptr);
            ASSERT(((uintptr_t)b & (align - 1)) == 0);

            memset(b, 0xff, size);

            doFree(b, &originalSize);
            ASSERT(originalSize == size);
        }
    }

    {
        size_t align = 65536;
        for (size_t size = align - 256; size <= align + 256; ++size)
        {
            void *b = doAllocation(size, align, scope);
            ASSERT(b != nullptr);
            ASSERT(((uintptr_t)b & (align - 1)) == 0);

            memset(b, 0xff, size);

            doFree(b, &originalSize);
            ASSERT(originalSize == size);
        }
    }

    for (size_t align : { 1, 4, 8, 4096 })
    {
        size_t size = 65536;

        void *b0 = doAllocation(size, align, scope);
        ASSERT(b0 != nullptr);
        strcpy((char *)b0, "Hello world");

        void *b1 = doReallocation(b0, size, align, scope, &originalSize);
        ASSERT(b1 != nullptr);
        ASSERT(originalSize == size);
        ASSERT(strcmp((char *)b1, "Hello world") == 0);

        void *b2 = doReallocation(b1, size * 2, align, scope, &originalSize);
        ASSERT(b2 != nullptr);
        ASSERT(originalSize == size);
        ASSERT(strcmp((char *)b2, "Hello world") == 0);

        void *b3 = doReallocation(b2, size, align, scope, &originalSize);
        ASSERT(b3 != nullptr);
        ASSERT(originalSize == size * 2);
        ASSERT(strcmp((char *)b3, "Hello world") == 0);

        doFree(b3, &originalSize);
        ASSERT(originalSize == size);
    }
}



void *DebugAllocationCallbacks::fnAllocation(void *pUserData,
    size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    const char *src = (const char *)pUserData;

    void *ret = doAllocation(size, alignment, allocationScope);

    LOGI("alloc: %s: %p: size=%lu alignment=%lu scope=%s",
        src, ret, (unsigned long)size, (unsigned long)alignment, scopeString(allocationScope));

    return ret;
}

void *DebugAllocationCallbacks::fnReallocation(void *pUserData,
    void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    const char *src = (const char *)pUserData;
    size_t originalSize;
    void *ret = doReallocation(pOriginal, size, alignment, allocationScope, &originalSize);

    LOGI("realloc: %s: %p -> %p: size=(original %lu, new %lu) alignment=%lu scope=%s",
        src, pOriginal, ret,
        (unsigned long)originalSize, (unsigned long)size,
        (unsigned long)alignment, scopeString(allocationScope));

    return ret;
}

void DebugAllocationCallbacks::fnFree(void *pUserData, void *pMemory)
{
    const char *src = (const char *)pUserData;
    size_t originalSize;
    doFree(pMemory, &originalSize);
    LOGI("free: %s: %p: size=%lu", src, pMemory, (unsigned long)originalSize);
}

void DebugAllocationCallbacks::fnInternalAllocation(void *pUserData,
    size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
    const char *src = (const char *)pUserData;
    LOGI("internal allocation: %s: size=%lu type=%s scope=%s",
        src, typeString(allocationType), scopeString(allocationScope));
}

void DebugAllocationCallbacks::fnInternalFree(void *pUserData,
    size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
    const char *src = (const char *)pUserData;
    LOGI("internal free: %s: size=%lu type=%s scope=%s",
        src, typeString(allocationType), scopeString(allocationScope));
}
