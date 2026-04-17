#include "vk_images.h"
#include "vk_initializers.h"

namespace vkutil {

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout,
                      VkImageLayout new_layout) {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
  barrier.oldLayout = current_layout;
  barrier.newLayout = new_layout;
  barrier.image = image;

  VkImageAspectFlags aspect = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
                                  ? VK_IMAGE_ASPECT_DEPTH_BIT
                                  : VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange = vkinit::image_subresource_range(aspect);

  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(cmd, &dep);
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
