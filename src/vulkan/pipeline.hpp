#pragma once

#include "vulkan/vulkan_raii.hpp"

#include "context.hpp"

class RenderPipeline {
public:
	RenderPipeline() = default;
	~RenderPipeline();

	RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    void createGraphicsPipeline(
        VulkanContext& context,
        const std::string& shaderPath,
        vk::PipelineLayout pipelineLayout,
        vk::Format colorFormat,
        vk::VertexInputBindingDescription bindingDescription,
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions
    );

    void createComputePipeline(
        VulkanContext& context,
        const std::string& shaderPath,
        vk::PipelineLayout pipelineLayout
    );

    void cleanup();

    const vk::raii::Pipeline& getGraphicsPipeline() const { return graphicsPipeline; }
    const vk::raii::Pipeline& getComputePipeline() const { return computePipeline; }

private:
    vk::raii::ShaderModule loadShaderModule(VulkanContext& context, const std::string& filepath);

    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::Pipeline computePipeline = nullptr;
};
