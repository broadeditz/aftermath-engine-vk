#include <iostream>
#include "computescreen.hpp"

void ComputeToScreen::createImage(VmaAllocator allocator, const vk::raii::Device& device, uint32_t w, uint32_t h) {
    width = w;
    height = h;

    std::cout << "Creating compute image: " << width << "x" << height << std::endl;

    // 1. Create image with VMA
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkImage vkImage;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &vkImage, &allocation, nullptr);
    image = vk::Image(vkImage);

    // 2. Create image view
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eR8G8B8A8Unorm;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    view = vk::raii::ImageView(device, viewInfo);

    // 3. Create sampler
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;

    sampler = vk::raii::Sampler(device, samplerInfo);
}

void ComputeToScreen::updateImageDescriptors(const vk::raii::Device& device) {
    // Update compute set - storage image
    vk::DescriptorImageInfo storageImageInfo;
    storageImageInfo.imageView = *view;
    storageImageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet storageImageWrite;
    storageImageWrite.dstSet = computeSet;
    storageImageWrite.dstBinding = 2;
    storageImageWrite.descriptorCount = 1;
    storageImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
    storageImageWrite.pImageInfo = &storageImageInfo;

    device.updateDescriptorSets(storageImageWrite, nullptr);

    // Update graphics set - sampled image and sampler
    vk::DescriptorImageInfo sampledImageInfo;
    sampledImageInfo.imageView = *view;
    sampledImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::DescriptorImageInfo samplerInfo;
    samplerInfo.sampler = *sampler;

    vk::WriteDescriptorSet graphicsWrites[2];
    graphicsWrites[0].dstSet = graphicsSet;
    graphicsWrites[0].dstBinding = 0;
    graphicsWrites[0].descriptorCount = 1;
    graphicsWrites[0].descriptorType = vk::DescriptorType::eSampledImage;
    graphicsWrites[0].pImageInfo = &sampledImageInfo;

    graphicsWrites[1].dstSet = graphicsSet;
    graphicsWrites[1].dstBinding = 1;
    graphicsWrites[1].descriptorCount = 1;
    graphicsWrites[1].descriptorType = vk::DescriptorType::eSampler;
    graphicsWrites[1].pImageInfo = &samplerInfo;

    device.updateDescriptorSets(graphicsWrites, nullptr);
}

void ComputeToScreen::create(VmaAllocator allocator, const vk::raii::Device& device, uint32_t queueFamilyIndex, uint32_t w, uint32_t h) {
    vmaAllocator = allocator;

    createImage(allocator, device, w, h);

    treeManager.initBuffers(allocator, *device, queueFamilyIndex);
    treeManager.createTestTree();
    treeManager.uploadToGPU();

    // 4. Create descriptor set layouts
    vk::DescriptorSetLayoutBinding computeBindings[3];

    // Binding 3: Tree storage buffer
    computeBindings[0].binding = 3;
    computeBindings[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[0].descriptorCount = 1;
    computeBindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[1] = {};
    computeBindings[1].binding = 4;  // Tree leaves
    computeBindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[1].descriptorCount = 1;
    computeBindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[2] = {};
    computeBindings[2].binding = 2;  // Storage image
    computeBindings[2].descriptorType = vk::DescriptorType::eStorageImage;
    computeBindings[2].descriptorCount = 1;
    computeBindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo computeLayoutInfo;
    computeLayoutInfo.bindingCount = 3;
    computeLayoutInfo.pBindings = computeBindings;

    computeLayout = vk::raii::DescriptorSetLayout(device, computeLayoutInfo);

    // Graphics layou
    vk::DescriptorSetLayoutBinding graphicsBindings[2];
    graphicsBindings[0].binding = 0;
    graphicsBindings[0].descriptorType = vk::DescriptorType::eSampledImage;
    graphicsBindings[0].descriptorCount = 1;
    graphicsBindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

    graphicsBindings[1].binding = 1;
    graphicsBindings[1].descriptorType = vk::DescriptorType::eSampler;
    graphicsBindings[1].descriptorCount = 1;
    graphicsBindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo graphicsLayoutInfo;
    graphicsLayoutInfo.bindingCount = 2;
    graphicsLayoutInfo.pBindings = graphicsBindings;

    graphicsLayout = vk::raii::DescriptorSetLayout(device, graphicsLayoutInfo);

    // 5. Create descriptor pool
    vk::DescriptorPoolSize poolSizes[4];

    // compute - storage buffer
    poolSizes[0].type = vk::DescriptorType::eStorageBuffer;
    poolSizes[0].descriptorCount = 2;

    // compute - storage image
    poolSizes[1].type = vk::DescriptorType::eStorageImage;
    poolSizes[1].descriptorCount = 1;

    // graphics - sampled image
    poolSizes[2].type = vk::DescriptorType::eSampledImage;
    poolSizes[2].descriptorCount = 1;

    // graphics - sampler
    poolSizes[3].type = vk::DescriptorType::eSampler;
    poolSizes[3].descriptorCount = 1;

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 4;
    poolInfo.pPoolSizes = poolSizes;

    pool = vk::raii::DescriptorPool(device, poolInfo);

    // 6. Allocate descriptor sets
    vk::DescriptorSetAllocateInfo allocInfoDesc;
    allocInfoDesc.descriptorPool = *pool;
    allocInfoDesc.descriptorSetCount = 1;
    allocInfoDesc.pSetLayouts = &(*computeLayout);

    computeSets = vk::raii::DescriptorSets(device, allocInfoDesc);
    computeSet = *computeSets[0];

    allocInfoDesc.pSetLayouts = &(*graphicsLayout);
    graphicsSets = vk::raii::DescriptorSets(device, allocInfoDesc);
    graphicsSet = *graphicsSets[0];

    // 7. Update descriptor sets
    // Tree buffer descriptor info
    // Tree nodes buffer
    VkDescriptorBufferInfo nodesBufferInfo{};
    nodesBufferInfo.buffer = treeManager.getNodeBuffer();
    nodesBufferInfo.offset = 0;
    nodesBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet nodesWrite{};
    nodesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    nodesWrite.dstSet = VkDescriptorSet(computeSet);
    nodesWrite.dstBinding = 3;
    nodesWrite.descriptorCount = 1;
    nodesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    nodesWrite.pBufferInfo = &nodesBufferInfo;

    // Tree leaves buffer
    VkDescriptorBufferInfo leavesBufferInfo{};
    leavesBufferInfo.buffer = treeManager.getLeafBuffer();
    leavesBufferInfo.offset = 0;
    leavesBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet leavesWrite{};
    leavesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    leavesWrite.dstSet = VkDescriptorSet(computeSet);
    leavesWrite.dstBinding = 4;
    leavesWrite.descriptorCount = 1;
    leavesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    leavesWrite.pBufferInfo = &leavesBufferInfo;

    // Storage image
    vk::DescriptorImageInfo storageImageInfo;
    storageImageInfo.imageView = *view;
    storageImageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet storageImageWrite;
    storageImageWrite.dstSet = computeSet;
    storageImageWrite.dstBinding = 2;
    storageImageWrite.descriptorCount = 1;
    storageImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
    storageImageWrite.pImageInfo = &storageImageInfo;

    VkWriteDescriptorSet writes[] = { nodesWrite, leavesWrite };
    vkUpdateDescriptorSets(view.getDevice(), 2, writes, 0, nullptr);

    // Update image and sampler descriptors
    updateImageDescriptors(device);

    frameData.create(device, allocator);
    descriptorSets = {
        computeSet,                    // set 0: storage buffer (tree) + storage image
        frameData.getDescriptorSet()   // set 1: frame uniforms
    };

    std::vector<vk::DescriptorSetLayout> setLayouts = {
        *computeLayout,                     // set 0
        frameData.getDescriptorSetLayout(), // set 1
    };

    // 8. Create pipeline layouts
    vk::PipelineLayoutCreateInfo computePipelineLayoutInfo;
    computePipelineLayoutInfo.setLayoutCount = 2;
    computePipelineLayoutInfo.pSetLayouts = setLayouts.data();

    computePipelineLayout = vk::raii::PipelineLayout(device, computePipelineLayoutInfo);

    vk::PipelineLayoutCreateInfo graphicsPipelineLayoutInfo;
    graphicsPipelineLayoutInfo.setLayoutCount = 1;
    graphicsPipelineLayoutInfo.pSetLayouts = &(*graphicsLayout);

    graphicsPipelineLayout = vk::raii::PipelineLayout(device, graphicsPipelineLayoutInfo);
}

void ComputeToScreen::destroy(VmaAllocator allocator) {
    treeManager.destroyBuffers();

    // RAII objects will be destroyed automatically
    vmaDestroyImage(allocator, VkImage(image), allocation);
    frameData.destroy();
}

void ComputeToScreen::resize(VmaAllocator allocator, const vk::raii::Device& device, uint32_t queueFamilyIndex, uint32_t w, uint32_t h) {
    vmaDestroyImage(allocator, VkImage(image), allocation);

    createImage(allocator, device, w, h);
    updateImageDescriptors(device);
}

void ComputeToScreen::initialTransition(const vk::raii::CommandBuffer& cmd) {
    vk::ImageMemoryBarrier barrier;
    barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eGeneral;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::DependencyFlags{},
        nullptr,
        nullptr,
        barrier
    );
}

void ComputeToScreen::recordCompute(const vk::raii::CommandBuffer& cmd, const vk::raii::Pipeline& computePipeline) {
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);

    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        *computePipelineLayout,
        0,
        descriptorSets,
        nullptr
    );

    uint32_t groupsX = (width + 15) / 16;
    uint32_t groupsY = (height + 15) / 16;
    cmd.dispatch(groupsX, groupsY, 1);

    vk::ImageMemoryBarrier barrier;
    barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    barrier.oldLayout = vk::ImageLayout::eGeneral;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::DependencyFlags{},
        nullptr,
        nullptr,
        barrier
    );
}

void ComputeToScreen::recordGraphics(const vk::raii::CommandBuffer& cmd, const vk::raii::Pipeline& graphicsPipeline) {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *graphicsPipelineLayout,
        0,
        graphicsSet,
        nullptr
    );
    cmd.draw(6, 1, 0, 0);
}

void ComputeToScreen::transitionBack(const vk::raii::CommandBuffer& cmd) {
    vk::ImageMemoryBarrier barrier;
    barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.newLayout = vk::ImageLayout::eGeneral;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::DependencyFlags{},
        nullptr,
        nullptr,
        barrier
    );
}