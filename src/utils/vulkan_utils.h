#pragma once
#include "pch.h"


namespace VulkanUtils {
    static void TransitionLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        VkDependencyInfo dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkroots::tables::LookupDeviceDispatch(cmdBuffer)->CmdPipelineBarrier2KHR(cmdBuffer, &dependencyInfo);
    }

    static void PipelineBarrier(VkCommandBuffer cmdBuffer, VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask) {
        VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
        barrier.srcAccessMask = srcAccessMask;
        barrier.srcStageMask = srcStageMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.dstStageMask = dstStageMask;

        VkDependencyInfo dependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR };
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &barrier;
        vkroots::tables::LookupDeviceDispatch(cmdBuffer)->CmdPipelineBarrier2KHR(cmdBuffer, &dependencyInfo);
    }

    static void DebugPipelineBarrier(VkCommandBuffer cmdBuffer) {
        return PipelineBarrier(cmdBuffer, VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR, VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);
    }

}