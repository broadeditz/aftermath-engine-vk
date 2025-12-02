#include "pipeline.hpp"

#include <fstream>

RenderPipeline::~RenderPipeline() {
    cleanup();
}

void RenderPipeline::cleanup() {
    graphicsPipeline = nullptr;
    computePipeline = nullptr;
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.size());
    return buffer;
}

vk::raii::ShaderModule RenderPipeline::loadShaderModule(VulkanContext& context, const std::string& filepath) {
    auto code = readFile(filepath);
    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };
    return vk::raii::ShaderModule(context.getDevice(), createInfo);
}

void RenderPipeline::createComputePipeline(
	VulkanContext& context,
	const std::string& shaderPath,
	vk::PipelineLayout pipelineLayout
) {
    vk::raii::ShaderModule shaderModule = loadShaderModule(context, shaderPath);

    vk::ComputePipelineCreateInfo pipelineInfo{
        .stage = {
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = *shaderModule,
            .pName = "computeMain"
        },
        .layout = pipelineLayout
    };

    computePipeline = vk::raii::Pipeline(context.getDevice(), nullptr, pipelineInfo);
}


void RenderPipeline::createGraphicsPipeline(
	VulkanContext& context,
    const std::string& shaderPath,
    vk::PipelineLayout pipelineLayout,
    vk::Format colorFormat,
    vk::VertexInputBindingDescription bindingDescription,
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions
) {
    vk::raii::ShaderModule shaderModule= loadShaderModule(context, shaderPath);

    vk::PipelineShaderStageCreateInfo shaderStages[] = {
        {.stage = vk::ShaderStageFlagBits::eVertex, .module = *shaderModule, .pName = "vertMain" },
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = *shaderModule, .pName = "fragMain" }
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .lineWidth = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat
    };

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .pNext = &renderingInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout
    };

    graphicsPipeline = vk::raii::Pipeline(context.getDevice(), nullptr, pipelineInfo);
}
