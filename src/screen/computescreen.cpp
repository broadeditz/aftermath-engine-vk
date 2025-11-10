#include <iostream>
#include "computescreen.hpp"

void ComputeToScreen::create(VmaAllocator allocator, const vk::raii::Device& device, uint32_t w, uint32_t h) {
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

    // 4. Create descriptor set layouts
    // Compute layout (storage image)
    vk::DescriptorSetLayoutBinding computeBinding;
    computeBinding.binding = 2;
    computeBinding.descriptorType = vk::DescriptorType::eStorageImage;
    computeBinding.descriptorCount = 1;
    computeBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo computeLayoutInfo;
    computeLayoutInfo.bindingCount = 1;
    computeLayoutInfo.pBindings = &computeBinding;

    computeLayout = vk::raii::DescriptorSetLayout(device, computeLayoutInfo);

    // Graphics layout (combined image sampler)
    vk::DescriptorSetLayoutBinding graphicsBindings[2];
    // Binding 0: Texture (sampled image)
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
    vk::DescriptorPoolSize poolSizes[3];
    // compute - storage image
    poolSizes[0].type = vk::DescriptorType::eStorageImage;
    poolSizes[0].descriptorCount = 1;
    // graphics - sampled image
    poolSizes[1].type = vk::DescriptorType::eSampledImage;
    poolSizes[1].descriptorCount = 1;
    // graphics - sampler
    poolSizes[2].type = vk::DescriptorType::eSampler;
    poolSizes[2].descriptorCount = 1;

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 3;
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
    // Compute set (storage image)
    vk::DescriptorImageInfo storageImageInfo;
    storageImageInfo.imageView = *view;
    storageImageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSet computeWrite;
    computeWrite.dstSet = computeSet;
    computeWrite.dstBinding = 2;
    computeWrite.descriptorCount = 1;
    computeWrite.descriptorType = vk::DescriptorType::eStorageImage;
    computeWrite.pImageInfo = &storageImageInfo;

    device.updateDescriptorSets(computeWrite, nullptr);

    // Graphics set (combined image sampler)
    vk::DescriptorImageInfo sampledImageInfo;
    sampledImageInfo.imageView = *view;
    sampledImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet graphicsWrites[2];
    // Graphics set - binding 0 (sampled image)
    graphicsWrites[0].dstSet = graphicsSet;
    graphicsWrites[0].dstBinding = 0;
    graphicsWrites[0].descriptorCount = 1;
    graphicsWrites[0].descriptorType = vk::DescriptorType::eSampledImage;
    graphicsWrites[0].pImageInfo = &sampledImageInfo;

    // Graphics set - binding 1 (sampler)
    vk::DescriptorImageInfo samplerWritesInfo;
    samplerWritesInfo.sampler = *sampler;

    graphicsWrites[1].dstSet = graphicsSet;
    graphicsWrites[1].dstBinding = 1;
    graphicsWrites[1].descriptorCount = 1;
    graphicsWrites[1].descriptorType = vk::DescriptorType::eSampler;
    graphicsWrites[1].pImageInfo = &samplerWritesInfo;

    device.updateDescriptorSets(graphicsWrites, nullptr);

    frameData.create(device, allocator);
    descriptorSets = {
        computeSet,                    // set 0: storage image
        frameData.getDescriptorSet()   // set 1: frame uniforms
    };

    std::vector<vk::DescriptorSetLayout> setLayouts = {
        *computeLayout, // set 0
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
    // RAII objects will be destroyed automatically
    // Only need to clean up VMA-managed resources
    vmaDestroyImage(allocator, VkImage(image), allocation);
    frameData.destroy();
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
        nullptr,  // memory barriers
        nullptr,  // buffer memory barriers
        barrier   // image memory barriers
    );
}

void ComputeToScreen::recordCompute(const vk::raii::CommandBuffer& cmd, const vk::raii::Pipeline& computePipeline) {
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);

    // Bind both descriptor sets in one call (set 0 and set 1)
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        *computePipelineLayout,
        0,  // firstSet = 0
        descriptorSets,
        nullptr  // dynamic offsets
    );

    uint32_t groupsX = (width + 15) / 16;
    uint32_t groupsY = (height + 15) / 16;
    cmd.dispatch(groupsX, groupsY, 1);

    // Barrier: GENERAL -> SHADER_READ_ONLY
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
        nullptr,  // memory barriers
        nullptr,  // buffer memory barriers
        barrier   // image memory barriers
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
        nullptr,  // memory barriers
        nullptr,  // buffer memory barriers
        barrier   // image memory barriers
    );
}