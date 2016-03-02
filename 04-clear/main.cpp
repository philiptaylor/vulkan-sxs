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

#include "common/Common.h"

#include "common/DeviceLoader.h"

#include <fstream>
#include <string>

static bool RunDemo()
{
    VkResult result;

    uint32_t imageWidth = 256;
    uint32_t imageHeight = 256;


    DeviceLoader loader;
    loader.SetDebugReportFlags(
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT);
    if (!loader.Setup())
        return false;

    VkDevice device = loader.GetDevice();

    const InstanceFunctions &pfn = loader.GetInstanceFunctions();

    VkPhysicalDeviceMemoryProperties memoryProperties;
    pfn.vkGetPhysicalDeviceMemoryProperties(loader.GetPhysicalDevice(), &memoryProperties);

    LOGI("Memory types: %d", memoryProperties.memoryTypeCount);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        auto type = memoryProperties.memoryTypes[i];
        std::string flags;
        if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) flags += "DEVICE_LOCAL|";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) flags += "HOST_VISIBLE|";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) flags += "HOST_COHERENT|";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) flags += "HOST_CACHED|";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) flags += "LAZILY_ALLOCATED|";
        if (flags.empty())
            flags = "0";
        else
            flags.pop_back();
        LOGI("    %d: heap %d, flags %s (0x%x)", i, type.heapIndex, flags.c_str(), type.propertyFlags);
    }

    LOGI("Memory heaps: %d", memoryProperties.memoryHeapCount);
    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i)
    {
        auto heap = memoryProperties.memoryHeaps[i];
        std::string flags;
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) flags += "DEVICE_LOCAL|";
        if (flags.empty())
            flags = "0";
        else
            flags.pop_back();
        LOGI("    %d: size %d MB, flags %s (0x%x)", i, heap.size / (1024*1024), flags.c_str(), heap.flags);
    }

    uint32_t deviceMemoryType = UINT32_MAX;
    uint32_t stagingMemoryType = UINT32_MAX;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        auto type = memoryProperties.memoryTypes[i];
        if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            if (deviceMemoryType == UINT32_MAX)
                deviceMemoryType = i;

        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            if (stagingMemoryType == UINT32_MAX)
                stagingMemoryType = i;
    }

    if (deviceMemoryType == UINT32_MAX || stagingMemoryType == UINT32_MAX)
    {
        LOGE("Failed to find suitable memory types");
        return false;
    }


    AutoVkImage stagingImage(pfn, device);
    AutoVkImage deviceImage(pfn, device);

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent = { imageWidth, imageHeight, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = nullptr;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    result = pfn.vkCreateImage(device, &imageCreateInfo, CREATE_ALLOCATOR(), stagingImage.ptr());
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateImage failed (%d)", result);
        return false;
    }

    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    result = pfn.vkCreateImage(device, &imageCreateInfo, CREATE_ALLOCATOR(), deviceImage.ptr());
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateImage failed (%d)", result);
        return false;
    }


    VkMemoryRequirements stagingImageMemReq;
    VkMemoryRequirements deviceImageMemReq;
    pfn.vkGetImageMemoryRequirements(device, stagingImage, &stagingImageMemReq);
    pfn.vkGetImageMemoryRequirements(device, deviceImage, &deviceImageMemReq);
    LOGI("Staging image: size=0x%x alignment=0x%x bits=0x%x",
        stagingImageMemReq.size, stagingImageMemReq.alignment, stagingImageMemReq.memoryTypeBits);
    LOGI("Device image: size=0x%x alignment=0x%x bits=0x%x",
        deviceImageMemReq.size, deviceImageMemReq.alignment, deviceImageMemReq.memoryTypeBits);

    if (!(stagingImageMemReq.memoryTypeBits & (1 << stagingMemoryType)))
    {
        LOGE("Staging image incompatible with the memory type we selected");
        return false;
    }

    if (!(deviceImageMemReq.memoryTypeBits & (1 << deviceMemoryType)))
    {
        LOGE("Device image incompatible with the memory type we selected");
        return false;
    }

    AutoVkDeviceMemory stagingImageMem(pfn, device);
    AutoVkDeviceMemory deviceImageMem(pfn, device);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    allocateInfo.allocationSize = stagingImageMemReq.size;
    allocateInfo.memoryTypeIndex = stagingMemoryType;
    result = pfn.vkAllocateMemory(device, &allocateInfo, CREATE_ALLOCATOR(), stagingImageMem.ptr());
    if (result != VK_SUCCESS)
    {
        LOGE("vkAllocateMemory failed (%d)", result);
        return false;
    }

    allocateInfo.allocationSize = deviceImageMemReq.size;
    allocateInfo.memoryTypeIndex = deviceMemoryType;
    result = pfn.vkAllocateMemory(device, &allocateInfo, CREATE_ALLOCATOR(), deviceImageMem.ptr());
    if (result != VK_SUCCESS)
    {
        LOGE("vkAllocateMemory failed (%d)", result);
        return false;
    }

    result = pfn.vkBindImageMemory(device, stagingImage, stagingImageMem, 0);
    if (result != VK_SUCCESS)
    {
        LOGE("vkBindImageMemory failed (%d)", result);
        return false;
    }

    result = pfn.vkBindImageMemory(device, deviceImage, deviceImageMem, 0);
    if (result != VK_SUCCESS)
    {
        LOGE("vkBindImageMemory failed (%d)", result);
        return false;
    }

    void *stagingImageMemPtr;
    result = pfn.vkMapMemory(device, stagingImageMem, 0, VK_WHOLE_SIZE, 0, &stagingImageMemPtr);
    if (result != VK_SUCCESS)
    {
        LOGE("vkMapMemory failed (%d)", result);
        return false;
    }


    VkImageSubresource colorSubresource;
    colorSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorSubresource.mipLevel = 0;
    colorSubresource.arrayLayer = 0;

    VkSubresourceLayout stagingImageLayout;
    pfn.vkGetImageSubresourceLayout(device, stagingImage, &colorSubresource, &stagingImageLayout);
    LOGI("Staging image: offset=0x%" PRIx64 " size=0x%" PRIx64 " pitch=(%" PRIu64 ", %" PRIu64 ", %" PRIu64 ")",
        stagingImageLayout.offset, stagingImageLayout.size,
        stagingImageLayout.rowPitch, stagingImageLayout.arrayPitch, stagingImageLayout.depthPitch);

    VkSubresourceLayout deviceImageLayout;
    pfn.vkGetImageSubresourceLayout(device, deviceImage, &colorSubresource, &deviceImageLayout);
    LOGI("Device image: offset=0x%" PRIx64 " size=0x%" PRIx64 " pitch=(%" PRIu64 ", %" PRIu64 ", %" PRIu64 ")",
        deviceImageLayout.offset, deviceImageLayout.size,
        deviceImageLayout.rowPitch, deviceImageLayout.arrayPitch, deviceImageLayout.depthPitch);



    AutoVkCommandPool graphicsCommandPool(pfn, device);
    AutoVkCommandPool transferCommandPool(pfn, device);

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = loader.GetGraphicsQueueFamily();
    result = pfn.vkCreateCommandPool(device, &commandPoolCreateInfo, CREATE_ALLOCATOR(), graphicsCommandPool.ptr());
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateCommandPool failed (%d)", result);
        return false;
    }

    commandPoolCreateInfo.queueFamilyIndex = loader.GetTransferQueueFamily();
    result = pfn.vkCreateCommandPool(device, &commandPoolCreateInfo, CREATE_ALLOCATOR(), transferCommandPool.ptr());
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateCommandPool failed (%d)", result);
        return false;
    }

    VkCommandBuffer clearCommandBuffer;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = graphicsCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    result = pfn.vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &clearCommandBuffer);
    if (result != VK_SUCCESS)
    {
        LOGE("vkAllocateCommandBuffers failed (%d)", result);
        return false;
    }

    VkCommandBuffer transferCommandBuffer;
    commandBufferAllocateInfo.commandPool = transferCommandPool;
    result = pfn.vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &transferCommandBuffer);
    if (result != VK_SUCCESS)
    {
        LOGE("vkAllocateCommandBuffers failed (%d)", result);
        return false;
    }



    VkClearColorValue clearColor;
    clearColor.float32[0] = 1.0f;
    clearColor.float32[1] = 0.65f;
    clearColor.float32[2] = 0.0f;
    clearColor.float32[3] = 1.0f;
    VkImageSubresourceRange colorSubresourceRange;
    colorSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorSubresourceRange.baseMipLevel = 0;
    colorSubresourceRange.levelCount = 1;
    colorSubresourceRange.baseArrayLayer = 0;
    colorSubresourceRange.layerCount = 1;


    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = pfn.vkBeginCommandBuffer(clearCommandBuffer, &beginInfo);
        if (result != VK_SUCCESS)
        {
            LOGE("vkBeginCommandBuffer failed (%d)", result);
            return false;
        }
    }

    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = deviceImage;
        barrier.subresourceRange = colorSubresourceRange;

        pfn.vkCmdPipelineBarrier(clearCommandBuffer,
            0,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    pfn.vkCmdClearColorImage(clearCommandBuffer, deviceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &colorSubresourceRange);

    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = loader.GetGraphicsQueueFamily();
        barrier.dstQueueFamilyIndex = loader.GetTransferQueueFamily();
        barrier.image = deviceImage;
        barrier.subresourceRange = colorSubresourceRange;

        pfn.vkCmdPipelineBarrier(clearCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    result = pfn.vkEndCommandBuffer(clearCommandBuffer);
    if (result != VK_SUCCESS)
    {
        LOGE("vkEndCommandBuffer failed (%d)", result);
        return false;
    }

    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = pfn.vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);
        if (result != VK_SUCCESS)
        {
            LOGE("vkBeginCommandBuffer failed (%d)", result);
            return false;
        }
    }

    {
        VkImageMemoryBarrier barriers[2] = {};
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = loader.GetGraphicsQueueFamily();
        barriers[0].dstQueueFamilyIndex = loader.GetTransferQueueFamily();
        barriers[0].image = deviceImage;
        barriers[0].subresourceRange = colorSubresourceRange;

        barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[1].srcAccessMask = 0;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].image = stagingImage;
        barriers[1].subresourceRange = colorSubresourceRange;

        pfn.vkCmdPipelineBarrier(transferCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            2, barriers);
    }

    {
        VkImageSubresourceLayers copySubresourceLayers = {};
        copySubresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copySubresourceLayers.mipLevel = 0;
        copySubresourceLayers.baseArrayLayer = 0;
        copySubresourceLayers.layerCount = 1;

        VkImageCopy copyRegion = {};
        copyRegion.srcSubresource = copySubresourceLayers;
        copyRegion.srcOffset = { 0, 0, 0 };
        copyRegion.dstSubresource = copySubresourceLayers;
        copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent = { imageWidth, imageHeight, 1 };

        pfn.vkCmdCopyImage(transferCommandBuffer,
            deviceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stagingImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion);
    }

    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = stagingImage;
        barrier.subresourceRange = colorSubresourceRange;

        pfn.vkCmdPipelineBarrier(transferCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    result = pfn.vkEndCommandBuffer(transferCommandBuffer);
    if (result != VK_SUCCESS)
    {
        LOGE("vkEndCommandBuffer failed (%d)", result);
        return false;
    }


    AutoVkSemaphore semaphore(pfn, device);
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = pfn.vkCreateSemaphore(device, &semaphoreCreateInfo, CREATE_ALLOCATOR(), semaphore.ptr());
    if (result != VK_SUCCESS)
    {
        LOGE("vkCreateSemaphore failed (%d)", result);
        return false;
    }

    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &clearCommandBuffer;

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = semaphore.ptr();

        result = pfn.vkQueueSubmit(loader.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            LOGE("vkQueueSubmit failed (%d)", result);
            return false;
        }
    }

    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &transferCommandBuffer;

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = semaphore.ptr();
        VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        submitInfo.pWaitDstStageMask = &stageFlags;

        result = pfn.vkQueueSubmit(loader.GetTransferQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            LOGE("vkQueueSubmit failed (%d)", result);
            return false;
        }
    }

    result = pfn.vkDeviceWaitIdle(device);
    if (result != VK_SUCCESS)
    {
        LOGE("vkDeviceWaitIdle failed (%d)", result);
        return false;
    }

    VkMappedMemoryRange stagingImageMemRange = {};
    stagingImageMemRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    stagingImageMemRange.memory = stagingImageMem;
    stagingImageMemRange.offset = 0;
    stagingImageMemRange.size = VK_WHOLE_SIZE;
    result = pfn.vkInvalidateMappedMemoryRanges(device, 1, &stagingImageMemRange);
    if (result != VK_SUCCESS)
    {
        LOGE("vkInvalidateMappedMemoryRanges failed (%d)", result);
        return false;
    }

    {
        std::ofstream out("output.tga", std::ofstream::binary | std::ofstream::out);

        const uint8_t tga_header[18] = {
            0, 0, 2,
            0, 0, 0, 0, 0,
            0, 0, 0, 0,
            (uint8_t)(imageWidth & 0xff), (uint8_t)(imageWidth>> 8),
            (uint8_t)(imageHeight & 0xff), (uint8_t)(imageHeight >> 8),
            32, 8 | (1 << 5),
        };
        out.write((const char *)tga_header, sizeof(tga_header));

        for (uint32_t y = 0; y < imageHeight; ++y)
        {
            uint8_t *row = (uint8_t *)stagingImageMemPtr + stagingImageLayout.offset + stagingImageLayout.rowPitch * y;
            for (uint32_t x = 0; x < imageWidth; ++x)
            {
                uint8_t rgba[4];
                memcpy(rgba, row + x * 4, 4);
                uint8_t bgra[4] = { rgba[2], rgba[1], rgba[0], rgba[3] };
                out.write((const char *)bgra, 4);
            }
        }
    }

    return true;
}

int main()
{
    if (!RunDemo())
        return -1;
    return 0;
}
