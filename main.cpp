#define GLFW_INCLUDE_VULKAN
// #define NDEBUG
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>

#include "utils.h"

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    return func != nullptr ? func(instance, pCreateInfo, pAllocator, pDebugMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, debugMessenger, pAllocator);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void *pUserData) {

    LOGE("Validation layer: " << pCallbackData->pMessage);
    return VK_FALSE;
}

static list<char> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        ERROR("Failed to open file!");

    size_t fileSize = file.tellg();
    list<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

class HelloTriangleApplication {
public:
void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

private:
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const list<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
const list<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

GLFWwindow *window;
VkInstance instance;

VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice device;
VkQueue graphicsQueue, presentQueue;

VkDebugUtilsMessengerEXT debugMessenger;
VkSurfaceKHR surface;

VkSwapchainKHR swapChain;
list<VkImage> swapChainImages;
list<VkImageView> swapChainImageViews;

VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;

VkRenderPass renderPass;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;

list<VkFramebuffer> swapChainFramebuffers;

VkCommandPool commandPool;
VkCommandBuffer commandBuffer;

void initWindow() {
    if (!glfwInit())
        LOGE("GLFW Initialization Error!");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    LOG("Initializing windows");
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    if (!window)
        LOGE("Window creation error!");
    else
        LOG("Window initialized!");
}

void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffer();
}

void mainLoop() {
    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    });

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame();
    }
}

void drawFrame() {}

void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
VkCommandBufferBeginInfo beginInfo{sType : VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    ERROR("Failed to begin recording command buffer!");

VkRenderPassBeginInfo renderPassInfo{sType : VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    // Area to render to
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearColor = {.color = {0.f, 0.f, 0.f, 1.f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    VkViewport viewport{};
    viewport.x = viewport.y = .0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = .0f;
    viewport.maxDepth = 1.f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        ERROR("Failed to record command buffer!");
}

void createCommandBuffer() {
VkCommandBufferAllocateInfo allocInfo{sType : VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
        ERROR("Failed to allocate command buffer!");
}

void createCommandPool() {
QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

VkCommandPoolCreateInfo poolInfo{sType : VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        ERROR("Failed to create command pool!");
}

void createFramebuffers() {
swapChainFramebuffers.resize(swapChainImageViews.size());

for (uint32_t i = 0; i < swapChainImageViews.size(); ++i) {
    VkFramebufferCreateInfo framebufferInfo{sType : VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &swapChainImageViews[i];
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
            ERROR("Failed to create framebuffer!");
    }
}

void createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear framebuffer before drawing
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store framebuffer after drawing
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Don't care about stencil
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Don't care about stencil

    // Layouts for the image before and after rendering
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Subpasses and attachment references
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment =
        0; // Index of the attachment in the attachment descriptions array
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint =
        VK_PIPELINE_BIND_POINT_GRAPHICS; // Subpass is a graphics subpass
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;             // Number of attachments
    renderPassInfo.pAttachments = &colorAttachment; // Attachments
    renderPassInfo.subpassCount = 1;                // Number of subpasses
    renderPassInfo.pSubpasses = &subpass;           // Subpasses

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
        VK_SUCCESS)
        ERROR("Failed to create render pass!");
}

void createGraphicsPipeline() {
list<char> vertShaderCode = readFile("shaders/vert.spv");
list<char> fragShaderCode = readFile("shaders/frag.spv");

LOG("size of vert.spv=" << vertShaderCode.size());
LOG("size of frag.spv=" << fragShaderCode.size());

VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

LOG("Shader modules created!");

VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

vertShaderStageInfo.module = vertShaderModule;
vertShaderStageInfo.pName = "main";

LOG("Vertex shader stage info created!");

VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

fragShaderStageInfo.module = fragShaderModule;
fragShaderStageInfo.pName = "main";

LOG("Fragment shader stage info created!");

VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

LOG("Shader stages created!");

list<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
dynamicStateInfo.dynamicStateCount = dynamicStates.size(); // TODO: Check if necessary cast
dynamicStateInfo.pDynamicStates = dynamicStates.data();

VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
vertexInputInfo.vertexBindingDescriptionCount = 0;
vertexInputInfo.vertexAttributeDescriptionCount = 0;

VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

VkViewport viewport{};
viewport.x = viewport.y = .0f;
viewport.width = (float)swapChainExtent.width;
viewport.height = (float)swapChainExtent.height;
viewport.minDepth = .0f;
viewport.maxDepth = 1.f;

VkRect2D scissor{};
scissor.offset = {0, 0};
scissor.extent = swapChainExtent;

VkPipelineViewportStateCreateInfo viewportStateInfo{};
viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
viewportStateInfo.viewportCount = 1;
viewportStateInfo.pViewports = &viewport;
viewportStateInfo.scissorCount = 1;
viewportStateInfo.pScissors = &scissor;

VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterizerInfo.depthClampEnable = VK_FALSE;
rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
rasterizerInfo.lineWidth = 1.f;
rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
rasterizerInfo.depthBiasEnable = VK_FALSE;

VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisamplingInfo.sampleShadingEnable = VK_FALSE;
multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

// TODO: Later come and play with color stuff, and more states

VkPipelineColorBlendAttachmentState colorBlendAttachment{};
colorBlendAttachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
colorBlendAttachment.blendEnable = VK_FALSE;

colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

VkPipelineColorBlendStateCreateInfo colorBlending{};
colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
colorBlending.logicOpEnable = VK_FALSE;
colorBlending.attachmentCount = 1;
colorBlending.pAttachments = &colorBlendAttachment;

VkPipelineLayoutCreateInfo pipelineLayoutInfo{sType : VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        ERROR("Failed to create pipeline layout!");

    LOG("Pipeline layout created!");

    VkGraphicsPipelineCreateInfo
    pipelineInfo{sType : VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineInfo.pViewportState = &viewportStateInfo;
        pipelineInfo.pRasterizationState = &rasterizerInfo;
        pipelineInfo.pMultisampleState = &multisamplingInfo;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStateInfo;

        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
            ERROR("Failed to create graphics pipeline!");

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    VkShaderModule createShaderModule(const list<char> &code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            ERROR("Failed to create shader module!");

        return shaderModule;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (uint32_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;

            createInfo.components.a = createInfo.components.g =
                createInfo.components.b = createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // Default

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
                ERROR("Failed to create image views!");
        }
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.maxImageCount > 0 && swapChainSupport.capabilities.maxImageCount < imageCount)
            imageCount = swapChainSupport.capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
            LOG("Concurrent sharing mode");
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            LOG("Exclusive sharing mode");
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Ignore channel
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // Ignore obscured pixels
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
            ERROR("Failed to create swap chain!");

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

        LOG("Created swapchain!");
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            ERROR("Failed to create window surface!");
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        list<VkDeviceQueueCreateInfo> queueCreateInfos;

        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t i : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.queueFamilyIndex = i;
            createInfo.queueCount = 1;
            createInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(createInfo);
        }

        LOG("Queues Set up!");

        VkPhysicalDeviceFeatures deviceFeatures{}; // Will come back to this later

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // No longer necessary since it uses the same validation layers as the
        // instance But keep it for old versions of Vulkan
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else 
        createInfo.enabledLayerCount = 0;

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
            ERROR("Failed to create logical device!");

        LOG("Logical device created!");
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
            ERROR("Failed to find any GPUs with Vulkan support!");

        list<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        LOG("Finding suitable device");

        std::multimap<int, VkPhysicalDevice> candidates;
        for (const auto &device : devices) {
            int score = rateDeviceSuitability(device);
            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.rbegin()->first > 0) {
            physicalDevice = candidates.rbegin()->second;

            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

            LOG("Found suitable device: " << deviceProperties.deviceName);
        } else
        ERROR("Failed to find a suitable GPU!");
    }

    int rateDeviceSuitability(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        LOG('\t' << deviceProperties.deviceName);

        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        // Application can't function without geometry shaders
        if (!deviceFeatures.geometryShader) return 0;

        int score = 0;

        // Discrete GPUs have a significant performance advantage
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        if (findQueueFamilies(device).isComplete())
            score += 1000;
        if (checkDeviceExtensionSupport(device)) {
            score += 1000;
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            if (swapChainSupport.formats.empty() ||
                swapChainSupport.presentModes.empty())
                return 0;
        }

        // Maximum possible size of textures affects graphics quality
        score += deviceProperties.limits.maxImageDimension2D;

        return score;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        list<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    void populateDebugMessengerCreateInfo(
        VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = 
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
            ERROR("Failed to set up debug messenger!");
    }

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        list<VkSurfaceFormatKHR> formats;
        list<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
            LOG("Formats: " << formatCount);
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, details.presentModes.data());
            LOG("Present modes: " << presentModeCount);
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const list<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return availableFormat;
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const list<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            actualExtent.width =
                std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                           capabilities.maxImageExtent.width);
            actualExtent.height =
                std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                           capabilities.maxImageExtent.height);

            LOG("Extent (0,0): " << actualExtent.width << 'x' << actualExtent.height);
            return actualExtent;
        }
        LOG("Extent: " << capabilities.currentExtent.width << 'x' << capabilities.currentExtent.height);
        return capabilities.currentExtent;
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        list<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size() && !indices.isComplete();
        ++i) {
            // Drawing and presentation in the same queue == performance
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }
        }

        return indices;
    }

    void setAvailableExtensions(list<VkExtensionProperties> &extensions) {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        extensions.resize(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        list<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers) {
            const auto layerFound = [layerName](VkLayerProperties &props) {
                return strcmp(props.layerName, layerName) == 0;
            };
            if (!std::any_of(availableLayers.begin(), availableLayers.end(), layerFound))
                return false;
        }

        return true;
    }

    list<const char *> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        list<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    // This is optional, but it helps the driver optimize for our application
    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport())
            ERROR("Validation layers requested, but not available!");

        LOG("Creating instance");
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        LOG("Application info created");

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        LOG("Info created");

        list<const char *> requiredExtensions = getRequiredExtensions();
        uint32_t extensionCount = static_cast<uint32_t>(requiredExtensions.size());

        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        } else
        createInfo.enabledLayerCount = 0;

        LOG("Checking for extensions");

        auto availableExtensions = list<VkExtensionProperties>();
        setAvailableExtensions(availableExtensions);

        const list<VkExtensionProperties>::iterator start = availableExtensions.begin(), end = availableExtensions.end();
        for (uint32_t i = 0; i < extensionCount; ++i) {
            const char *extension = requiredExtensions[i];

            const auto includesExtension = [extension](VkExtensionProperties &props) {
                return strcmp(props.extensionName, extension) == 0;
            };

            if (!std::any_of(start, end, includesExtension)) {
                LOGE("Extension " << extension << " is not available");
                break;
            }
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            ERROR("Failed to create instance!");

        LOG("Instance created!");
    }

    void cleanup() {
        LOG("Cleaning up");

        vkDestroyCommandPool(device, commandPool, nullptr);
        LOG("Command pool destroyed!");

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        LOG("Graphics pipeline destroyed!");

        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        LOG("Pipeline layout destroyed!");

        vkDestroyRenderPass(device, renderPass, nullptr);
        LOG("Render pass destroyed!");

        for (auto &framebuffer : swapChainFramebuffers)
        vkDestroyFramebuffer(device, framebuffer, nullptr);

        for (auto &imageView : swapChainImageViews)
        vkDestroyImageView(device, imageView, nullptr);
        LOG("Image views destroyed!");

        vkDestroySwapchainKHR(device, swapChain, nullptr);
        LOG("Swapchain destroyed!");

        vkDestroyDevice(device, nullptr);
        LOG("Logical Device destroyed!");

        vkDestroySurfaceKHR(instance, surface, nullptr);
        LOG("Surface destroyed!");

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            LOG("Debug Messenger destroyed!");
        }

        vkDestroyInstance(instance, nullptr);
        LOG("Instance destroyed!");

        glfwDestroyWindow(window);
        LOG("Window destroyed");

        glfwTerminate();
        LOG("GLFW terminated");
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOGE(e.what());
        return EXIT_FAILURE;
    }
}
