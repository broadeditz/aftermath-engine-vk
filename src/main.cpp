#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

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
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::SurfaceKHR surface = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    vk::raii::Device device = nullptr;
    VmaAllocator allocator = nullptr;

    vk::raii::Queue graphicsQueue = nullptr;
    vk::raii::Queue presentQueue = nullptr;
    uint32_t graphicsIndex;
    uint32_t presentIndex;

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR swapChainSurfaceFormat;
    vk::Extent2D swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

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
        cleanupSwapChain();
        computeScreen.destroy(allocator);
        vmaDestroyAllocator(allocator);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void initVulkan() {
		std::cout << "creating Vulkan instance" << std::endl;
        createInstance();
		std::cout << "creating window surface" << std::endl;
        createSurface();
		std::cout << "picking physical device" << std::endl;
        pickPhysicalDevice();
		std::cout << "creating logical device" << std::endl;
        createLogicalDevice();
		std::cout << "creating swap chain" << std::endl;
        createSwapChain();
		std::cout << "creating image views" << std::endl;
        createImageViews();
        std::cout << "creating tranfer command pool" << std::endl;
        //createTransferCommandPool();
		std::cout << "creating command pool" << std::endl;
        createCommandPool();
		std::cout << "creating command buffers" << std::endl;
        createCommandBuffers();
		std::cout << "creating compute screen" << std::endl;
        // TODO: initialTransition on computeScreen
        computeScreen.create(allocator, device, graphicsIndex, swapChainExtent.width, swapChainExtent.height);
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

    void createInstance() {
        // *** FIRST: Initialize with vkGetInstanceProcAddr ***
        VULKAN_HPP_DEFAULT_DISPATCHER.init(context.getDispatcher()->vkGetInstanceProcAddr);
    
        vk::ApplicationInfo appInfo{
            .pApplicationName = "Eldritch Aftermath",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName = "Aftermath Engine",
            .engineVersion = VK_MAKE_VERSION(0, 0, 1),
            .apiVersion = vk::ApiVersion14
        };
    
        std::vector<char const*> requiredLayers;
        if (dev) requiredLayers = validationLayers;
    
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions,
        };
    
        instance = vk::raii::Instance(context, createInfo);
    
        // *** THEN: Initialize with the instance ***
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    }

    void createSurface() {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    void pickPhysicalDevice() {
        auto devices = instance.enumeratePhysicalDevices();
        if (devices.empty()) throw std::runtime_error("no Vulkan GPUs found!");
        physicalDevice = std::move(devices[0]);
        std::cout << "Using: " << physicalDevice.getProperties().deviceName << std::endl;
    }

    void createLogicalDevice() {
        auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        graphicsIndex = 0;
        for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
            if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphicsIndex = i;
                break;
            }
        }

        presentIndex = physicalDevice.getSurfaceSupportKHR(graphicsIndex, *surface) ? graphicsIndex : graphicsIndex;

        vk::PhysicalDeviceVulkan12Features vulkan12Features{
            .shaderInt8 = vk::True,
            .uniformAndStorageBuffer8BitAccess = vk::True,
            .storageBuffer8BitAccess = vk::True
        };

        vk::PhysicalDeviceVulkan13Features vulkan13Features{
            .pNext = &vulkan12Features,
            .synchronization2 = vk::True,
            .dynamicRendering = vk::True
        };

        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = graphicsIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &vulkan13Features,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data()
        };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);

		VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

        graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
        presentQueue = graphicsQueue;

        VmaAllocatorCreateInfo allocatorInfo{
            .physicalDevice = *physicalDevice,
            .device = *device,
            .instance = *instance,
            .vulkanApiVersion = VK_API_VERSION_1_4
        };
        vmaCreateAllocator(&allocatorInfo, &allocator);
    }

    void createSwapChain() {
        auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        auto formats = physicalDevice.getSurfaceFormatsKHR(*surface);
        auto presentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

        swapChainSurfaceFormat = formats[0];
        for (const auto& format : formats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                swapChainSurfaceFormat = format;
                break;
            }
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        swapChainExtent = {
            std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) imageCount = std::min(imageCount, capabilities.maxImageCount);

        vk::SwapchainCreateInfoKHR createInfo{
            .surface = *surface,
            .minImageCount = imageCount,
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = vk::PresentModeKHR::eFifo,
            .clipped = vk::True
        };

        swapChain = device.createSwapchainKHR(createInfo);
        swapChainImages = swapChain.getImages();
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();
        cleanupSwapChain();
        createSwapChain();
        createImageViews();
        computeScreen.resize(allocator, device, graphicsIndex, swapChainExtent.width, swapChainExtent.height);
        //computeScreen.destroy(allocator);
        //computeScreen.create(allocator, device, graphicsIndex, swapChainExtent.width, swapChainExtent.height);
    }

    void cleanupSwapChain() {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    void createImageViews() {
        swapChainImageViews.clear();
        for (auto image : swapChainImages) {
            vk::ImageViewCreateInfo createInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = swapChainSurfaceFormat.format,
                .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
            };
            swapChainImageViews.emplace_back(device, createInfo);
        }
    }

    void createComputePipeline() {
        auto shaderCode = readFile("shaders/slang.spv");
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = shaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
        };
        vk::raii::ShaderModule shaderModule(device, createInfo);

        vk::ComputePipelineCreateInfo pipelineInfo{
            .stage = {
                .stage = vk::ShaderStageFlagBits::eCompute,
                .module = *shaderModule,
                .pName = "computeMain"
            },
            .layout = *computeScreen.computePipelineLayout
        };

        computePipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
    }

    void createGraphicsPipeline() {
        auto shaderCode = readFile("shaders/slang.spv");
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = shaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
        };
        vk::raii::ShaderModule shaderModule(device, createInfo);

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

        graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
    }

    void createVertexBuffer() {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        vk::BufferCreateInfo bufferInfo{ .size = bufferSize, .usage = vk::BufferUsageFlagBits::eVertexBuffer };
        vertexBuffer = vk::raii::Buffer(device, bufferInfo);

        auto memReq = vertexBuffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{
            .allocationSize = memReq.size,
            .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        };
        vertexBufferMemory = vk::raii::DeviceMemory(device, allocInfo);
        vertexBuffer.bindMemory(*vertexBufferMemory, 0);

        void* data = vertexBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, vertices.data(), bufferSize);
        vertexBufferMemory.unmapMemory();
    }

    void createIndexBuffer() {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        vk::BufferCreateInfo bufferInfo{ .size = bufferSize, .usage = vk::BufferUsageFlagBits::eIndexBuffer };
        indexBuffer = vk::raii::Buffer(device, bufferInfo);

        auto memReq = indexBuffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{
            .allocationSize = memReq.size,
            .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        };
        indexBufferMemory = vk::raii::DeviceMemory(device, allocInfo);
        indexBuffer.bindMemory(*indexBufferMemory, 0);

        void* data = indexBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, indices.data(), bufferSize);
        indexBufferMemory.unmapMemory();
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        auto memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createTransferCommandPool() {
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient,  // Optimized for short-lived buffers
            .queueFamilyIndex = graphicsIndex
        };
        transferCommandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createCommandPool() {
        vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = graphicsIndex
        };
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT
        };
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
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
            .image = swapChainImages[imageIndex],
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        };
        commandBuffers[currentFrame].pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
            });

        // Render pass
        vk::RenderingAttachmentInfo colorAttachment{
            .imageView = *swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
        };

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
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
            renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
        }
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            inFlightFences.emplace_back(device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
        }
    }

    void drawFrame() {
        device.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX);

        if (lastTime + std::chrono::seconds(1) <= std::chrono::steady_clock::now()) {
            std::cout << "FPS: " << frameCounter << std::endl;
            frameCounter = 0;
            lastTime = std::chrono::steady_clock::now();
        }

        auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[semaphoreIndex], nullptr);
        if (result == vk::Result::eErrorOutOfDateKHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
            presentCompleteSemaphores[semaphoreIndex] = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo{});
            return;
        }

        auto currentTime = std::chrono::high_resolution_clock::now(); 
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();

        float time = std::chrono::duration<float>(currentTime - startTime).count();

        camera.update(deltaTime);
        computeScreen.frameData.update(FrameUniforms{
            .time = time,
            .aperture = 0.01,
            .focusDistance = 3.5,
            .fov = 1.5,
            // TODO: pass position & direction from FPS controls
            .cameraPosition = camera.getPosition(),
            .cameraDirection = camera.getDirection(),
        });

        device.resetFences(*inFlightFences[currentFrame]);
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
        graphicsQueue.submit(submitInfo, *inFlightFences[currentFrame]);

        vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*renderFinishedSemaphores[semaphoreIndex],
            .swapchainCount = 1,
            .pSwapchains = &*swapChain,
            .pImageIndices = &imageIndex
        };

        try {
            result = presentQueue.presentKHR(presentInfo);
            if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
                framebufferResized = false;
                recreateSwapChain();
            }
        }
        catch (const vk::SystemError& e) {
            if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR)) {
                recreateSwapChain();
            }
        }

        semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphores.size();
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        frameCounter++;
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        device.waitIdle();
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