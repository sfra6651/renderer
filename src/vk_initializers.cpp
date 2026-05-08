#include "vk_initializers.h"

namespace vkinit {

VkCommandPoolCreateInfo command_pool_create_info(uint32_t queue_family_index,
                                                 VkCommandPoolCreateFlags flags) {
  VkCommandPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.queueFamilyIndex = queue_family_index;
  info.flags = flags;
  return info;
}

VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count) {
  VkCommandBufferAllocateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.commandPool = pool;
  info.commandBufferCount = count;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  return info;
}

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags) {
  VkFenceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.flags = flags;
  return info;
}

VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags) {
  VkSemaphoreCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.flags = flags;
  return info;
}

VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags) {
  VkCommandBufferBeginInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.flags = flags;
  return info;
}

VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd) {
  VkCommandBufferSubmitInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.commandBuffer = cmd;
  info.deviceMask = 0;
  return info;
}

VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore) {
  VkSemaphoreSubmitInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  info.semaphore = semaphore;
  info.stageMask = stage_mask;
  info.deviceIndex = 0;
  info.value = 1;
  return info;
}

VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signal_sem,
                          VkSemaphoreSubmitInfo* wait_sem) {
  VkSubmitInfo2 info{};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  info.waitSemaphoreInfoCount = wait_sem ? 1 : 0;
  info.pWaitSemaphoreInfos = wait_sem;
  info.signalSemaphoreInfoCount = signal_sem ? 1 : 0;
  info.pSignalSemaphoreInfos = signal_sem;
  info.commandBufferInfoCount = cmd ? 1 : 0;
  info.pCommandBufferInfos = cmd;
  return info;
}

VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent) {
  VkImageCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.imageType = VK_IMAGE_TYPE_2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels = 1;
  info.arrayLayers = 1;

  //for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
  info.samples = VK_SAMPLE_COUNT_1_BIT;

  //optimal tiling, which means the image is stored on the best gpu format
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usageFlags;

  return info;
}

VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags) {
  // build a image-view for the depth image to use for rendering
  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = nullptr;

  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = image;
  info.format = format;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspectFlags;

  return info;

}

VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect_mask) {
  VkImageSubresourceRange range{};
  range.aspectMask = aspect_mask;
  range.baseMipLevel = 0;
  range.levelCount = VK_REMAINING_MIP_LEVELS;
  range.baseArrayLayer = 0;
  range.layerCount = VK_REMAINING_ARRAY_LAYERS;
  return range;
}

VkRenderingAttachmentInfo attachment_info(
  VkImageView view,
  VkClearValue* clear,
  VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/
) {
  VkRenderingAttachmentInfo colorAttachment {};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.pNext = nullptr;

  colorAttachment.imageView = view;
  colorAttachment.imageLayout = layout;
  colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (clear) {
    colorAttachment.clearValue = *clear;
  }

  return colorAttachment;
}

VkRenderingInfo rendering_info(
  VkExtent2D render_extent,
  VkRenderingAttachmentInfo* color_attachment,
  VkRenderingAttachmentInfo* depth_attachment
) {
  VkRenderingInfo renderInfo {};
  renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderInfo.pNext = nullptr;

  renderInfo.renderArea = VkRect2D { VkOffset2D { 0, 0 }, render_extent };
  renderInfo.layerCount = 1;
  renderInfo.colorAttachmentCount = 1;
  renderInfo.pColorAttachments = color_attachment;
  renderInfo.pDepthAttachment = depth_attachment;
  renderInfo.pStencilAttachment = nullptr;

  return renderInfo;
}

VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(
  VkShaderStageFlagBits stage,
  VkShaderModule shader_module,
  const char* entry
) {
  VkPipelineShaderStageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.pNext = nullptr;
  info.stage = stage;
  info.module = shader_module;
  info.pName = entry;
  return info;
}

VkPipelineLayoutCreateInfo pipeline_layout_create_info() {
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.setLayoutCount = 0;
  info.pSetLayouts = nullptr;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges = nullptr;
  return info;
}

}  // namespace vkinit
