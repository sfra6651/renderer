#include "vk_descriptors.h"

#include "vk_types.h"
#include "vulkan/vulkan_core.h"

VkDescriptorPool DescriptorAllocator::get_pool(VkDevice device)
{
  VkDescriptorPool newPool;

  if (this->readyPools.size() != 0) {
    newPool = readyPools.back();
    readyPools.pop_back();
  }
  else {
    newPool = this->create_pool(device, setsPerPool, ratios);

    this->setsPerPool = this->setsPerPool * 1.5;

    if (this->setsPerPool > 4092) {
      this->setsPerPool = 4092;
    }
  }

  return newPool;
}

VkDescriptorPool DescriptorAllocator::create_pool(
  VkDevice device,
  uint32_t setsCount,
  std::span<PoolSizeRatio> poolRatios
) {
  std::vector<VkDescriptorPoolSize> poolSizes;

  for(PoolSizeRatio ratio : poolRatios) {
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = ratio.type;
    poolSize.descriptorCount = static_cast<uint32_t>(ratio.ratio * setsCount);

    poolSizes.push_back(poolSize);
  }

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = 0;
  poolInfo.maxSets = setsCount;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();

  VkDescriptorPool newPool;
  vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
  return newPool;
}

void DescriptorAllocator::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
  this->ratios.clear();

  for (auto r : poolRatios) {
    this->ratios.push_back(r);
  }

  VkDescriptorPool newPool = this->create_pool(device, maxSets, poolRatios);

  this->setsPerPool = maxSets * 1.5;

  readyPools.push_back(newPool);
}


void DescriptorAllocator::clear_pools(VkDevice device)
{ 
  for (auto p : readyPools) {
    vkResetDescriptorPool(device, p, 0);
  }
  for (auto p : fullPools) {
    vkResetDescriptorPool(device, p, 0);
    readyPools.push_back(p);
  }
  fullPools.clear();
}

void DescriptorAllocator::destroy_pools(VkDevice device)
{
  for (auto p: this->readyPools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }

  this->readyPools.clear();
  for (auto p : this->fullPools) {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  this->fullPools.clear();
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
  VkDescriptorPool poolToUse = get_pool(device);

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.pNext = pNext;
  allocInfo.descriptorPool = poolToUse;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VkDescriptorSet ds;
  VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

  //allocation failed. try again
  if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
    this->fullPools.push_back(poolToUse);

    poolToUse = get_pool(device);
    allocInfo.descriptorPool = poolToUse;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
  }

  readyPools.push_back(poolToUse);
  return ds;
}


void DescriptorWriter::write_buffer(
  int binding,
  VkBuffer buffer,
  size_t size,
  size_t offset,
  VkDescriptorType type
) {
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buffer;
  bufferInfo.offset = offset;
  bufferInfo.range = size;

  VkDescriptorBufferInfo& info = this->bufferInfos.emplace_back(bufferInfo);

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = &info;

  this->writes.push_back(write);
}

void DescriptorWriter::write_image(
  int binding,
  VkImageView image,
  VkSampler sampler,
  VkImageLayout layout,
  VkDescriptorType type
) {
  VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	VkWriteDescriptorSet write  {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::clear()
{
  imageInfos.clear();
  writes.clear();
  bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
  for (VkWriteDescriptorSet& write : writes) {
    write.dstSet = set;
  }

  vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}


void vkutil::DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) 
{
  VkDescriptorSetLayoutBinding newbind {};
  newbind.binding = binding;
  newbind.descriptorCount = 1;
  newbind.descriptorType = type;

  this->bindings.push_back(newbind);
}


void vkutil::DescriptorLayoutBuilder::clear()
{
  this->bindings.clear();
}

VkDescriptorSetLayout vkutil::DescriptorLayoutBuilder::build(
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

void vkutil::DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) 
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

void vkutil::DescriptorAllocator::clear_descriptors(VkDevice device)
{
  vkResetDescriptorPool(device, pool, 0);
}

void vkutil::DescriptorAllocator::destroy_pool(VkDevice device)
{
  vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet vkutil::DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
  VkDescriptorSetAllocateInfo allocInfo {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.pNext = nullptr;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VkDescriptorSet ds;
  VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

  return ds;
}


