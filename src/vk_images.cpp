#include "vk_images.h"
#include "vk_initializers.h"

namespace vkutil {

void transition_image(
  VkCommandBuffer cmd,
  VkImage image,
  VkImageLayout currentLayout,
  VkImageLayout newLayout
) {
  VkImageMemoryBarrier2 imageBarrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  imageBarrier.pNext = nullptr;

  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  imageBarrier.oldLayout = currentLayout;
  imageBarrier.newLayout = newLayout;

  VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
  imageBarrier.image = image;

  VkDependencyInfo depInfo {};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.pNext = nullptr;

  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &imageBarrier;

  vkCmdPipelineBarrier2(cmd, &depInfo);



}

void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination,
                         VkExtent2D src_size, VkExtent2D dst_size) {
  VkImageBlit2 blit{};
  blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
  blit.srcOffsets[1].x = static_cast<int32_t>(src_size.width);
  blit.srcOffsets[1].y = static_cast<int32_t>(src_size.height);
  blit.srcOffsets[1].z = 1;
  blit.dstOffsets[1].x = static_cast<int32_t>(dst_size.width);
  blit.dstOffsets[1].y = static_cast<int32_t>(dst_size.height);
  blit.dstOffsets[1].z = 1;
  blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.srcSubresource.layerCount = 1;
  blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.dstSubresource.layerCount = 1;

  VkBlitImageInfo2 info{};
  info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
  info.srcImage = source;
  info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  info.dstImage = destination;
  info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  info.filter = VK_FILTER_LINEAR;
  info.regionCount = 1;
  info.pRegions = &blit;

  vkCmdBlitImage2(cmd, &info);
}

}  // namespace vkutil
