#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "camera/camera.hpp"
#include "screen/computescreen.hpp"
#include "vulkan/context.hpp"
#include "vulkan/swapchain.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

struct Vertex {
    glm::vec2 pos;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        return { {
            {.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, pos) },
            {.location = 1, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, texCoord) }
        } };
    }
};

const std::vector<Vertex> vertices = {
    {{-1.0f, -1.0f}, {1.0f, 1.0f}},
    {{1.0f, -1.0f},  {0.0f, 1.0f}},
    {{1.0f, 1.0f},   {0.0f, 0.0f}},
    {{-1.0f, 1.0f},  {1.0f, 0.0f}}
};

const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open file: " + filename);

    std::vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.size());
    return buffer;
}

const bool dev = true;
const std::vector<char const*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
constexpr int MAX_FRAMES_IN_FLIGHT = 1;

class MainApplication {
public:
    void run() {
		std::cout << "initializing window" << std::endl;
        initWindow();
		std::cout << "initializing Vulkan" << std::endl;
        initVulkan();
		std::cout << "entering main loop" << std::endl;
        mainLoop();
		std::cout << "cleaning up" << std::endl;
        cleanup();
    }

    GLFWwindow* window;

private:
	VulkanContext context;
	vk::raii::SurfaceKHR surface = nullptr;

	SwapChainManager swapchainManager;

    vk::raii::PipelineLayout graphicsPipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::Pipeline computePipeline = nullptr;

    ComputeToScreen computeScreen;

    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;
    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::DeviceMemory indexBufferMemory = nullptr;

    vk::raii::CommandPool commandPool = nullptr;
    vk::raii::CommandBuffers commandBuffers = nullptr;

    vk::raii::CommandPool transferCommandPool = nullptr;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;

    bool framebufferResized = false;
    uint32_t currentFrame = 0;
    uint32_t semaphoreIndex = 0;

    FPSCamera camera = nullptr;

    uint32_t frameCounter = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastSecond = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::steady_clock::now();

    std::vector<const char*> deviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSynchronization2ExtensionName
    };

    std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(1280, 720, "Eldritch Aftermath", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int, int) {
            reinterpret_cast<MainApplication*>(glfwGetWindowUserPointer(w))->framebufferResized = true;
        });
    }

    void cleanup() {
        swapchainManager.cleanupSwapChain();
        computeScreen.destroy(context.getAllocator());
        vmaDestroyAllocator(context.getAllocator());
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void initVulkan() {
		std::cout << "creating Vulkan instance" << std::endl;
		context.createInstance(dev);
		std::cout << "creating window surface" << std::endl;
        createSurface();
		std::cout << "initializing context" << std::endl;
		context.init(*surface);
		std::cout << "initializing swap chain" << std::endl;
		int width = 0, height = 0;
    	glfwGetFramebufferSize(window, &width, &height);
        swapchainManager.init(context, width, height, *surface);
		std::cout << "creating command pool" << std::endl;
        createCommandPool();
		std::cout << "creating command buffers" << std::endl;
        createCommandBuffers();
		std::cout << "creating compute screen" << std::endl;
        // TODO: initialTransition on computeScreen
        computeScreen.create(context.getAllocator(), context.getDevice(), context.getGraphicsQueueIndex(), swapchainManager.getSwapChainExtent().width, swapchainManager.getSwapChainExtent().height);
		std::cout << "creating compute pipeline" << std::endl;
        createComputePipeline();
		std::cout << "creating graphics pipeline" << std::endl;
        createGraphicsPipeline();
		std::cout << "creating vertex buffer" << std::endl;
        createVertexBuffer();
		std::cout << "creating index buffer" << std::endl;
        createIndexBuffer();
		std::cout << "creating synchronization objects" << std::endl;
        createSyncObjects();
        camera = FPSCamera(window);
    }

    void createSurface() {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*context.getInstance(), window, nullptr, &_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(context.getInstance(), _surface);
    }

    void createComputePipeline() {
        auto shaderCode = readFile("shaders/slang.spv");
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = shaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
        };
        vk::raii::ShaderModule shaderModule(context.getDevice(), createInfo);

        vk::ComputePipelineCreateInfo pipelineInfo{
            .stage = {
                .stage = vk::ShaderStageFlagBits::eCompute,
                .module = *shaderModule,
                .pName = "computeMain"
            },
            .layout = *computeScreen.computePipelineLayout
        };

        computePipeline = vk::raii::Pipeline(context.getDevice(), nullptr, pipelineInfo);
    }

    void createGraphicsPipeline() {
        auto shaderCode = readFile("shaders/slang.spv");
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = shaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
        };
        vk::raii::ShaderModule shaderModule(context.getDevice(), createInfo);

        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            {.stage = vk::ShaderStageFlagBits::eVertex, .module = *shaderModule, .pName = "vertMain" },
            {.stage = vk::ShaderStageFlagBits::eFragment, .module = *shaderModule, .pName = "fragMain" }
        };

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

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

        vk::SurfaceFormatKHR swapChainSurfaceFormat = swapchainManager.getSwapChainSurfaceFormat();

        vk::PipelineRenderingCreateInfo renderingInfo{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapChainSurfaceFormat.format
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
            .layout = *computeScreen.graphicsPipelineLayout
        };

        graphicsPipeline = vk::raii::Pipeline(context.getDevice(), nullptr, pipelineInfo);
    }

    void createVertexBuffer() {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        vk::BufferCreateInfo bufferInfo{ .size = bufferSize, .usage = vk::BufferUsageFlagBits::eVertexBuffer };
        vertexBuffer = vk::raii::Buffer(context.getDevice(), bufferInfo);

        auto memReq = vertexBuffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{
            .allocationSize = memReq.size,
            .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        };
        vertexBufferMemory = vk::raii::DeviceMemory(context.getDevice(), allocInfo);
        vertexBuffer.bindMemory(*vertexBufferMemory, 0);

        void* data = vertexBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, vertices.data(), bufferSize);
        vertexBufferMemory.unmapMemory();
    }

    void createIndexBuffer() {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        vk::BufferCreateInfo bufferInfo{ .size = bufferSize, .usage = vk::BufferUsageFlagBits::eIndexBuffer };
        indexBuffer = vk::raii::Buffer(context.getDevice(), bufferInfo);

        auto memReq = indexBuffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{
            .allocationSize = memReq.size,
            .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        };
        indexBufferMemory = vk::raii::DeviceMemory(context.getDevice(), allocInfo);
        indexBuffer.bindMemory(*indexBufferMemory, 0);

        void* data = indexBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, indices.data(), bufferSize);
        indexBufferMemory.unmapMemory();
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        auto memProperties = context.getPhysicalDevice().getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createCommandPool() {
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = context.getGraphicsQueueIndex()
        };
        commandPool = vk::raii::CommandPool(context.getDevice(), poolInfo);
    }

    void createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT
        };
        commandBuffers = vk::raii::CommandBuffers(context.getDevice(), allocInfo);
    }

    void recordCommandBuffer(uint32_t imageIndex) {
        commandBuffers[currentFrame].begin({});

        // Run compute shader
        computeScreen.recordCompute(commandBuffers[currentFrame], computePipeline);

        // Transition swapchain image
        vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = swapchainManager.getSwapChainImages()[imageIndex],
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        };
        commandBuffers[currentFrame].pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
            });

        // Render pass
        vk::RenderingAttachmentInfo colorAttachment{
            .imageView = *swapchainManager.getSwapChainImageViews()[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
        };

        vk::Extent2D swapChainExtent = swapchainManager.getSwapChainExtent();

        vk::RenderingInfo renderingInfo{
            .renderArea = { {0, 0}, swapChainExtent },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
        };

        commandBuffers[currentFrame].beginRendering(renderingInfo);
        commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
            *computeScreen.graphicsPipelineLayout, 0, computeScreen.graphicsSet, {});
        commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, { 0 });
        commandBuffers[currentFrame].bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);
        commandBuffers[currentFrame].setViewport(0, vk::Viewport{
            0, 0, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0, 1
            });
        commandBuffers[currentFrame].setScissor(0, vk::Rect2D{ {0, 0}, swapChainExtent });
        commandBuffers[currentFrame].drawIndexed(indices.size(), 1, 0, 0, 0);
        commandBuffers[currentFrame].endRendering();

        // Transition to present
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
        barrier.dstAccessMask = {};
        barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        commandBuffers[currentFrame].pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
            });

        computeScreen.transitionBack(commandBuffers[currentFrame]);
        commandBuffers[currentFrame].end();
    }

    void createSyncObjects() {
        for (size_t i = 0; i < swapchainManager.getSwapChainImages().size(); i++) {
            presentCompleteSemaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo{});
            renderFinishedSemaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo{});
        }
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            inFlightFences.emplace_back(context.getDevice(), vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
        }
    }

    void drawFrame() {
        context.getDevice().waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX);

        auto [result, imageIndex] = swapchainManager.getSwapChain().acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[semaphoreIndex], nullptr);
        if (result == vk::Result::eErrorOutOfDateKHR || framebufferResized) {
            framebufferResized = false;

            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            while (width == 0 || height == 0) {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            }
            swapchainManager.recreateSwapChain(context, width, height);

            computeScreen.resize(context.getAllocator(), context.getDevice(), context.getGraphicsQueueIndex(), swapchainManager.getSwapChainExtent().width, swapchainManager.getSwapChainExtent().height);
            presentCompleteSemaphores[semaphoreIndex] = vk::raii::Semaphore(context.getDevice(), vk::SemaphoreCreateInfo{});
            return;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();

        float time = std::chrono::duration<float>(currentTime - startTime).count();

        camera.update(deltaTime);

        computeScreen.treeManager.moveObserver({
            camera.getPosition().x,
            camera.getPosition().y,
            camera.getPosition().z,
        });

        if (lastSecond + std::chrono::seconds(1) <= std::chrono::steady_clock::now()) {
            std::cout << "FPS: " << frameCounter << std::endl;
            std::cout << camera.getPosition().x << ", " << camera.getPosition().y << ", " << camera.getPosition().z << std::endl;
            frameCounter = 0;
            lastSecond = std::chrono::steady_clock::now();
        }

        computeScreen.frameData.update(FrameUniforms{
            .time = time,
            .aperture = 0.001,
            .focusDistance = 3.5,
            .fov = 1.5,
            .cameraPosition = camera.getPosition(),
            .cameraDirection = camera.getDirection(),
        });

        context.getDevice().resetFences(*inFlightFences[currentFrame]);
        commandBuffers[currentFrame].reset();
        recordCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*presentCompleteSemaphores[semaphoreIndex],
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffers[currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*renderFinishedSemaphores[semaphoreIndex]
        };
        context.getGraphicsQueue().submit(submitInfo, *inFlightFences[currentFrame]);

        vk::raii::SwapchainKHR& swapChain = swapchainManager.getSwapChain();

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*renderFinishedSemaphores[semaphoreIndex],
            .swapchainCount = 1,
            .pSwapchains = &*swapChain,
            .pImageIndices = &imageIndex
        };

        try {
            result = context.getPresentQueue().presentKHR(presentInfo);
            if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
                framebufferResized = false;
                int width = 0, height = 0;
                glfwGetFramebufferSize(window, &width, &height);
                while (width == 0 || height == 0) {
                    glfwGetFramebufferSize(window, &width, &height);
                    glfwWaitEvents();
                }
                swapchainManager.recreateSwapChain(context, width, height);

                computeScreen.resize(context.getAllocator(), context.getDevice(), context.getGraphicsQueueIndex(), swapchainManager.getSwapChainExtent().width, swapchainManager.getSwapChainExtent().height);
            }
        }
        catch (const vk::SystemError& e) {
            if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR)) {
	            int width = 0, height = 0;
	            glfwGetFramebufferSize(window, &width, &height);
	            while (width == 0 || height == 0) {
	                glfwGetFramebufferSize(window, &width, &height);
	                glfwWaitEvents();
	            }
	            swapchainManager.recreateSwapChain(context, width, height);

	            computeScreen.resize(context.getAllocator(), context.getDevice(), context.getGraphicsQueueIndex(), swapchainManager.getSwapChainExtent().width, swapchainManager.getSwapChainExtent().height);
            }
        }

        semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphores.size();
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        frameCounter++;
        lastTime = currentTime;
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        context.getDevice().waitIdle();
		std::cout << "Main loop exited" << std::endl;
    }
};

int main() {
    std::cout << "Starting application..." << std::endl;
    std::cout.flush();

    try {
        std::cout << "Creating application object..." << std::endl;
        MainApplication app;

        std::cout << "Running application..." << std::endl;
        app.run();

        std::cout << "Application finished normally" << std::endl;
    }
    catch (const vk::SystemError& e) {
        std::cerr << "Vulkan error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.code() << std::endl;
        std::cerr.flush();
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr.flush();
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "Unknown exception caught!" << std::endl;
        std::cerr.flush();
        return EXIT_FAILURE;
    }

    std::cout << "Exiting cleanly" << std::endl;
    return EXIT_SUCCESS;
}
