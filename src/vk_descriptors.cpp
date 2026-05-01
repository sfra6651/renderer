#include "vk_descriptors.h"

#include "vk_types.h"
#include "vulkan/vulkan_core.h"


void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) 
{
  VkDescriptorSetLayoutBinding newbind {};
  newbind.binding = binding;
  newbind.descriptorCount = 1;
  newbind.descriptorType = type;

  this->bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
  this->bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(
  VkDevice device,
  VkShaderStageFlags shaderStages,
  void* pNext,
  VkDescriptorSetLayoutCreateFlags flags
)
{
  for (auto& b : this->bindings) {
    b.stageFlags |= shaderStages;
  }

  VkDescriptorSetLayoutCreateInfo info {};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pBindings = bindings.data();
  info.bindingCount = this->bindings.size();
  info.flags = flags;

  VkDescriptorSetLayout set;
  VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

  return set;
};

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) 
{
  std::vector<VkDescriptorPoolSize> poolSizes;

  for (PoolSizeRatio ratio : poolRatios) {
    poolSizes.push_back(VkDescriptorPoolSize {
      .type = ratio.type,
      .descriptorCount = uint32_t(ratio.ratio * maxSets)
    });
  }

  VkDescriptorPoolCreateInfo poolInfo {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = 0;
  poolInfo.maxSets = maxSets;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();

  vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);

}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
  vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
  vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
  VkDescriptorSetAllocateInfo allocInfo {};
  allocInfo.pNext = nullptr;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VkDescriptorSet ds;
  VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

  return ds;
}


