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

#ifndef INCLUDED_VKSXS_INSTANCE_FUNCTIONS
#define INCLUDED_VKSXS_INSTANCE_FUNCTIONS

#include "common/Common.h"

#define INSTANCE_FUNCTIONS \
    X(vkAllocateCommandBuffers) \
    X(vkAllocateDescriptorSets) \
    X(vkAllocateMemory) \
    X(vkBeginCommandBuffer) \
    X(vkBindBufferMemory) \
    X(vkBindImageMemory) \
    X(vkCmdBeginQuery) \
    X(vkCmdBeginRenderPass) \
    X(vkCmdBindDescriptorSets) \
    X(vkCmdBindIndexBuffer) \
    X(vkCmdBindPipeline) \
    X(vkCmdBindVertexBuffers) \
    X(vkCmdBlitImage) \
    X(vkCmdClearAttachments) \
    X(vkCmdClearColorImage) \
    X(vkCmdClearDepthStencilImage) \
    X(vkCmdCopyBuffer) \
    X(vkCmdCopyBufferToImage) \
    X(vkCmdCopyImage) \
    X(vkCmdCopyImageToBuffer) \
    X(vkCmdCopyQueryPoolResults) \
    X(vkCmdDispatch) \
    X(vkCmdDispatchIndirect) \
    X(vkCmdDraw) \
    X(vkCmdDrawIndexed) \
    X(vkCmdDrawIndexedIndirect) \
    X(vkCmdDrawIndirect) \
    X(vkCmdEndQuery) \
    X(vkCmdEndRenderPass) \
    X(vkCmdExecuteCommands) \
    X(vkCmdFillBuffer) \
    X(vkCmdNextSubpass) \
    X(vkCmdPipelineBarrier) \
    X(vkCmdPushConstants) \
    X(vkCmdResetEvent) \
    X(vkCmdResetQueryPool) \
    X(vkCmdResolveImage) \
    X(vkCmdSetBlendConstants) \
    X(vkCmdSetDepthBias) \
    X(vkCmdSetDepthBounds) \
    X(vkCmdSetEvent) \
    X(vkCmdSetLineWidth) \
    X(vkCmdSetScissor) \
    X(vkCmdSetStencilCompareMask) \
    X(vkCmdSetStencilReference) \
    X(vkCmdSetStencilWriteMask) \
    X(vkCmdSetViewport) \
    X(vkCmdUpdateBuffer) \
    X(vkCmdWaitEvents) \
    X(vkCmdWriteTimestamp) \
    X(vkCreateBuffer) \
    X(vkCreateBufferView) \
    X(vkCreateCommandPool) \
    X(vkCreateComputePipelines) \
    X(vkCreateDescriptorPool) \
    X(vkCreateDescriptorSetLayout) \
    X(vkCreateDevice) \
    X(vkCreateEvent) \
    X(vkCreateFence) \
    X(vkCreateFramebuffer) \
    X(vkCreateGraphicsPipelines) \
    X(vkCreateImage) \
    X(vkCreateImageView) \
    X(vkCreatePipelineCache) \
    X(vkCreatePipelineLayout) \
    X(vkCreateQueryPool) \
    X(vkCreateRenderPass) \
    X(vkCreateSampler) \
    X(vkCreateSemaphore) \
    X(vkCreateShaderModule) \
    X(vkDestroyBuffer) \
    X(vkDestroyBufferView) \
    X(vkDestroyCommandPool) \
    X(vkDestroyDescriptorPool) \
    X(vkDestroyDescriptorSetLayout) \
    X(vkDestroyDevice) \
    X(vkDestroyEvent) \
    X(vkDestroyFence) \
    X(vkDestroyFramebuffer) \
    X(vkDestroyImage) \
    X(vkDestroyImageView) \
    X(vkDestroyInstance) \
    X(vkDestroyPipeline) \
    X(vkDestroyPipelineCache) \
    X(vkDestroyPipelineLayout) \
    X(vkDestroyQueryPool) \
    X(vkDestroyRenderPass) \
    X(vkDestroySampler) \
    X(vkDestroySemaphore) \
    X(vkDestroyShaderModule) \
    X(vkDeviceWaitIdle) \
    X(vkEndCommandBuffer) \
    X(vkEnumerateDeviceExtensionProperties) \
    X(vkEnumerateDeviceLayerProperties) \
    X(vkEnumeratePhysicalDevices) \
    X(vkFlushMappedMemoryRanges) \
    X(vkFreeCommandBuffers) \
    X(vkFreeDescriptorSets) \
    X(vkFreeMemory) \
    X(vkGetBufferMemoryRequirements) \
    X(vkGetDeviceMemoryCommitment) \
    X(vkGetDeviceProcAddr) \
    X(vkGetDeviceQueue) \
    X(vkGetEventStatus) \
    X(vkGetFenceStatus) \
    X(vkGetImageMemoryRequirements) \
    X(vkGetImageSparseMemoryRequirements) \
    X(vkGetImageSubresourceLayout) \
    X(vkGetInstanceProcAddr) \
    X(vkGetPhysicalDeviceFeatures) \
    X(vkGetPhysicalDeviceFormatProperties) \
    X(vkGetPhysicalDeviceImageFormatProperties) \
    X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkGetPhysicalDeviceProperties) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) \
    X(vkGetPhysicalDeviceSparseImageFormatProperties) \
    X(vkGetPipelineCacheData) \
    X(vkGetQueryPoolResults) \
    X(vkGetRenderAreaGranularity) \
    X(vkInvalidateMappedMemoryRanges) \
    X(vkMapMemory) \
    X(vkMergePipelineCaches) \
    X(vkQueueBindSparse) \
    X(vkQueueSubmit) \
    X(vkQueueWaitIdle) \
    X(vkResetCommandBuffer) \
    X(vkResetCommandPool) \
    X(vkResetDescriptorPool) \
    X(vkResetEvent) \
    X(vkResetFences) \
    X(vkSetEvent) \
    X(vkUnmapMemory) \
    X(vkUpdateDescriptorSets) \
    X(vkWaitForFences) \

#define INSTANCE_FUNCTIONS_EXT_DEBUG_REPORT \
    X(vkCreateDebugReportCallbackEXT) \
    X(vkDestroyDebugReportCallbackEXT) \
    X(vkDebugReportMessageEXT) \

struct InstanceFunctions
{
#define X(n) PFN_##n n;
    INSTANCE_FUNCTIONS
    INSTANCE_FUNCTIONS_EXT_DEBUG_REPORT
#undef X
};

#endif // INCLUDED_VKSXS_INSTANCE_FUNCTIONS
