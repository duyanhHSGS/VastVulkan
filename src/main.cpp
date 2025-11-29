#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <windows.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>

// MASSIVE OVERRIDE! HEAP POLIIIIICE!
void* operator new(std::size_t) {
    std::cerr << "[ALERT] Heap allocation detected! Birdy hates heap! exploding...\n";
    std::abort();
}
void* operator new[](std::size_t) {
    std::cerr << "[ALERT] Heap array detected! Birdy hates heap! exploding...\n";
    std::abort();
}
void operator delete(void*) noexcept {}
void operator delete(void*, std::size_t) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete[](void*, std::size_t) noexcept {}

inline void VK_CHECK(VkResult err, const char* file = __builtin_FILE(), int line = __builtin_LINE()) {
    if (err != VK_SUCCESS) {
        std::cerr << "Vulkan error: " << err << " at "
                  << file << ":" << line << "\n";
        std::abort();
    }
}
static const uint32_t WIDTH = 1600;
static const uint32_t HEIGHT = 500;

// === Minimal math (column-major 4x4 matrix) =========================

struct Mat4 {
    float m[16];
};

static Mat4 mat_identity() {  // unit
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static Mat4 mat_perspective(float fovyRad, float aspect, float zNear, float zFar) {
    Mat4 r{};
    float f = 1.0f / std::tan(fovyRad / 2.0f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return r;
}

static Mat4 mat_rotate_y(float angle) {
    Mat4 r = mat_identity();
    float c = std::cos(angle);
    float s = std::sin(angle);
    r.m[0] = c;
    r.m[2] = s;
    r.m[8] = -s;
    r.m[10] = c;
    return r;
}

static Mat4 mat_translate(float x, float y, float z) {
    Mat4 r = mat_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

// static Mat4 mat_mul(const Mat4& a, const Mat4& b) {
//     Mat4 r{};
//     for (int row = 0; row < 4; ++row) {
//         for (int col = 0; col < 4; ++col) {
//             r.m[col + row * 4] =
//                 a.m[0 + row * 4] * b.m[col + 0 * 4] +
//                 a.m[1 + row * 4] * b.m[col + 1 * 4] +
//                 a.m[2 + row * 4] * b.m[col + 2 * 4] +
//                 a.m[3 + row * 4] * b.m[col + 3 * 4];
//         }
//     }
//     return r;
// }

static Mat4 mat_mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    // column-major storage: index = col*4 + row
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                // A[row,k] is a.m[k*4 + row]
                // B[k,col] is b.m[col*4 + k]
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

// === Vertex data ====================================================

struct Vertex {
    float pos[3];
    float color[3];
};

// 3 vertices for one triangle
static const Vertex VERTICES[3] = {
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
};

// === Win32 globals ==================================================

static HINSTANCE g_hInstance = nullptr;
static HWND g_hWnd = nullptr;
static bool g_running = true;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// === Vulkan objects (globals, no dynamic containers) ===============

static VkInstance g_instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static uint32_t g_graphicsQueueFamily = 0;
static VkQueue g_graphicsQueue = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static VkFormat g_swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
static VkExtent2D g_swapchainExtent = {WIDTH, HEIGHT};

// Swapchain images (we'll cap at 4 statically)
static VkImage g_swapchainImages[4];
static VkImageView g_swapchainImageViews[4];
static uint32_t g_swapchainImageCount = 0;

// Render pass, framebuffers
static VkRenderPass g_renderPass = VK_NULL_HANDLE;
static VkFramebuffer g_framebuffers[4];

// Command pool & buffers
static VkCommandPool g_commandPool = VK_NULL_HANDLE;
static VkCommandBuffer g_commandBuffers[4];

// Sync
static VkSemaphore g_imageAvailableSemaphores[2];
static VkSemaphore g_renderFinishedSemaphores[2];
static VkFence g_inFlightFences[2];
static uint32_t g_currentFrame = 0;

// Pipeline
static VkPipelineLayout g_pipelineLayout = VK_NULL_HANDLE;
static VkPipeline g_graphicsPipeline = VK_NULL_HANDLE;

// Vertex buffer & memory
static VkBuffer g_vertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_vertexBufferMemory = VK_NULL_HANDLE;

// Uniform buffer & memory
static VkBuffer g_uniformBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_uniformBufferMemory = VK_NULL_HANDLE;

// Descriptor set layout, pool, set
static VkDescriptorSetLayout g_descriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet g_descriptorSet = VK_NULL_HANDLE;

// Timing
static std::chrono::steady_clock::time_point g_startTime;

// === Helper functions ===============================================

static void create_win32_window() {
    const wchar_t CLASS_NAME[] = L"VulkanNoHeapWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassW(&wc);

    RECT rect = {0, 0, (LONG)WIDTH, (LONG)HEIGHT};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Vulkan Spinning Triangle (No Heap)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        g_hInstance,
        nullptr);
}

static void create_instance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "NoHeapTriangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "NoEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const char* extensions[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"};

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &g_instance));
}

static void pick_physical_device() {
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(g_instance, &deviceCount, nullptr));
    if (deviceCount == 0) {
        std::cerr << "No Vulkan physical devices found.\n";
        std::abort();
    }

    // Static array to avoid dynamic allocation
    VkPhysicalDevice devices[16];
    if (deviceCount > 16) deviceCount = 16;
    VK_CHECK(vkEnumeratePhysicalDevices(g_instance, &deviceCount, devices));

    for (uint32_t i = 0; i < deviceCount; ++i) {
        // Pick the first one with a graphics queue & present support
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, nullptr);
        VkQueueFamilyProperties qprops[16];
        if (queueFamilyCount > 16) queueFamilyCount = 16;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, qprops);

        for (uint32_t j = 0; j < queueFamilyCount; ++j) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, g_surface, &presentSupport);
            if ((qprops[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                g_physicalDevice = devices[i];
                g_graphicsQueueFamily = j;
                return;
            }
        }
    }

    std::cerr << "Failed to find suitable GPU.\n";
    std::abort();
}

static void create_logical_device() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = g_graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {
        "VK_KHR_swapchain"};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    VK_CHECK(vkCreateDevice(g_physicalDevice, &createInfo, nullptr, &g_device));
    vkGetDeviceQueue(g_device, g_graphicsQueueFamily, 0, &g_graphicsQueue);
}

static void create_surface() {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = g_hInstance;
    createInfo.hwnd = g_hWnd;

    VK_CHECK(vkCreateWin32SurfaceKHR(g_instance, &createInfo, nullptr, &g_surface));
}

static void create_swapchain() {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_surface, &caps));

    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, nullptr));
    VkSurfaceFormatKHR formats[16];
    if (formatCount > 16) formatCount = 16;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, formats));

    g_swapchainImageFormat = formats[0].format;
    if (formats[0].format == VK_FORMAT_UNDEFINED && formatCount > 1) {
        g_swapchainImageFormat = formats[1].format;
    }

    if (caps.currentExtent.width != UINT32_MAX) {
        g_swapchainExtent = caps.currentExtent;
    } else {
        g_swapchainExtent.width = WIDTH;
        g_swapchainExtent.height = HEIGHT;
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }
    if (imageCount > 4) imageCount = 4;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = g_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = g_swapchainImageFormat;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = g_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(g_device, &createInfo, nullptr, &g_swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, nullptr));
    if (g_swapchainImageCount > 4) g_swapchainImageCount = 4;
    VK_CHECK(vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, g_swapchainImages));

    for (uint32_t i = 0; i < g_swapchainImageCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = g_swapchainImageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(g_device, &viewInfo, nullptr, &g_swapchainImageViews[i]));
    }
}

static void create_render_pass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = g_swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(g_device, &renderPassInfo, nullptr, &g_renderPass));
}

static void create_framebuffers() {
    for (uint32_t i = 0; i < g_swapchainImageCount; ++i) {
        VkImageView attachments[] = {
            g_swapchainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = g_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = g_swapchainExtent.width;
        framebufferInfo.height = g_swapchainExtent.height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(g_device, &framebufferInfo, nullptr, &g_framebuffers[i]));
    }
}

static void create_command_pool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = g_graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(g_device, &poolInfo, nullptr, &g_commandPool));
}

static void create_command_buffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = g_swapchainImageCount;

    VK_CHECK(vkAllocateCommandBuffers(g_device, &allocInfo, g_commandBuffers));
}

static void create_sync_objects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < 2; ++i) {
        VK_CHECK(vkCreateSemaphore(g_device, &semInfo, nullptr, &g_imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(g_device, &semInfo, nullptr, &g_renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(g_device, &fenceInfo, nullptr, &g_inFlightFences[i]));
    }
}

// Helper to read SPIR-V from file into a static buffer (max 64KB) to avoid heap.
struct ShaderData {
    uint32_t words[16 * 1024];  // 64KB / 4
    size_t size;
};

static ShaderData read_shader_file(const char* path) {
    ShaderData data{};
    FILE* f = nullptr;
    fopen_s(&f, path, "rb");
    if (!f) {
        std::cerr << "Failed to open shader file: " << path << "\n";
        std::abort();
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (long)sizeof(data.words)) {
        std::cerr << "Shader file size invalid or too big: " << path << "\n";
        fclose(f);
        std::abort();
    }
    size_t read = fread(data.words, 1, (size_t)len, f);
    fclose(f);
    if (read != (size_t)len) {
        std::cerr << "Failed to read entire shader file: " << path << "\n";
        std::abort();
    }
    data.size = (size_t)len;
    return data;
}

static VkShaderModule create_shader_module(const ShaderData& data) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = data.size;
    createInfo.pCode = data.words;

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(g_device, &createInfo, nullptr, &module));
    return module;
}

static void create_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(g_device, &layoutInfo, nullptr, &g_descriptorSetLayout));
}

static uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(g_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    std::cerr << "Failed to find suitable memory type.\n";
    std::abort();
}

static void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags props,
                          VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(g_device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(g_device, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, props);

    VK_CHECK(vkAllocateMemory(g_device, &allocInfo, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(g_device, buffer, memory, 0));
}

static void create_vertex_buffer() {
    VkDeviceSize bufferSize = sizeof(VERTICES);

    create_buffer(bufferSize,
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  g_vertexBuffer, g_vertexBufferMemory);

    void* data = nullptr;
    VK_CHECK(vkMapMemory(g_device, g_vertexBufferMemory, 0, bufferSize, 0, &data));
    std::memcpy(data, VERTICES, (size_t)bufferSize);
    vkUnmapMemory(g_device, g_vertexBufferMemory);
}

static void create_uniform_buffer() {
    VkDeviceSize bufferSize = sizeof(Mat4);

    create_buffer(bufferSize,
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  g_uniformBuffer, g_uniformBufferMemory);
}

static void create_descriptor_pool_and_set() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VK_CHECK(vkCreateDescriptorPool(g_device, &poolInfo, nullptr, &g_descriptorPool));

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &g_descriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(g_device, &allocInfo, &g_descriptorSet));

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = g_uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Mat4);

    VkWriteDescriptorSet descWrite{};
    descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrite.dstSet = g_descriptorSet;
    descWrite.dstBinding = 0;
    descWrite.dstArrayElement = 0;
    descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descWrite.descriptorCount = 1;
    descWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(g_device, 1, &descWrite, 0, nullptr);
}

static void create_graphics_pipeline() {
    ShaderData vertData = read_shader_file("triangle.vert.spv");
    ShaderData fragData = read_shader_file("triangle.frag.spv");

    VkShaderModule vertModule = create_shader_module(vertData);
    VkShaderModule fragModule = create_shader_module(fragData);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    // Vertex input
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc[2]{};
    attrDesc[0].binding = 0;
    attrDesc[0].location = 0;
    attrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDesc[0].offset = offsetof(Vertex, pos);

    attrDesc[1].binding = 0;
    attrDesc[1].location = 1;
    attrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDesc[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attrDesc;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_swapchainExtent.width;
    viewport.height = (float)g_swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_descriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, nullptr, &g_pipelineLayout));

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_pipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_graphicsPipeline));

    vkDestroyShaderModule(g_device, vertModule, nullptr);
    vkDestroyShaderModule(g_device, fragModule, nullptr);
}

static void record_command_buffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkClearValue clearColor{};
    clearColor.color = {{0.1f, 0.1f, 0.2f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = g_renderPass;
    renderPassInfo.framebuffer = g_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = g_swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphicsPipeline);

    VkBuffer vertexBuffers[] = {g_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            g_pipelineLayout, 0, 1, &g_descriptorSet, 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));
}

static void update_uniform_buffer() {
    auto now = std::chrono::steady_clock::now();
    float time = std::chrono::duration<float>(now - g_startTime).count();

    Mat4 proj = mat_perspective(45.0f * 3.14159265f / 180.0f,
                                (float)g_swapchainExtent.width / (float)g_swapchainExtent.height,
                                0.1f, 10.0f);
    Mat4 view = mat_translate(0.0f, 0.0f, -2.0f);
    Mat4 model = mat_rotate_y(time * 10);

    Mat4 mvp = mat_mul(proj, mat_mul(view, model));

    void* data = nullptr;
    VK_CHECK(vkMapMemory(g_device, g_uniformBufferMemory, 0, sizeof(Mat4), 0, &data));
    std::memcpy(data, &mvp, sizeof(Mat4));
    vkUnmapMemory(g_device, g_uniformBufferMemory);
}

static void draw_frame() {
    vkWaitForFences(g_device, 1, &g_inFlightFences[g_currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_inFlightFences[g_currentFrame]);

    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX,
                                         g_imageAvailableSemaphores[g_currentFrame],
                                         VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        return;  // window resize not handled in this minimal example
    }
    VK_CHECK(res);

    vkResetCommandBuffer(g_commandBuffers[imageIndex], 0);
    record_command_buffer(g_commandBuffers[imageIndex], imageIndex);

    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphores[g_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphores[g_currentFrame]};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, g_inFlightFences[g_currentFrame]));

    VkSwapchainKHR swapchains[] = {g_swapchain};

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    res = vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        // Ignore resize for simplicity
    } else {
        VK_CHECK(res);
    }

    g_currentFrame = (g_currentFrame + 1) % 2;
}

// === Cleanup ========================================================

static void cleanup() {
    vkDeviceWaitIdle(g_device);

    for (uint32_t i = 0; i < 2; ++i) {
        vkDestroySemaphore(g_device, g_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(g_device, g_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(g_device, g_inFlightFences[i], nullptr);
    }

    vkDestroyBuffer(g_device, g_vertexBuffer, nullptr);
    vkFreeMemory(g_device, g_vertexBufferMemory, nullptr);

    vkDestroyBuffer(g_device, g_uniformBuffer, nullptr);
    vkFreeMemory(g_device, g_uniformBufferMemory, nullptr);

    vkDestroyDescriptorPool(g_device, g_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(g_device, g_descriptorSetLayout, nullptr);

    vkDestroyPipeline(g_device, g_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(g_device, g_pipelineLayout, nullptr);

    for (uint32_t i = 0; i < g_swapchainImageCount; ++i) {
        vkDestroyFramebuffer(g_device, g_framebuffers[i], nullptr);
        vkDestroyImageView(g_device, g_swapchainImageViews[i], nullptr);
    }

    vkDestroyRenderPass(g_device, g_renderPass, nullptr);
    vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
    vkDestroyCommandPool(g_device, g_commandPool, nullptr);

    vkDestroyDevice(g_device, nullptr);
    vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
    vkDestroyInstance(g_instance, nullptr);

    DestroyWindow(g_hWnd);
}

// === Win32 WndProc ==================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            g_running = false;
            return 0;
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

// === WinMain / main =================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    g_hInstance = hInstance;

    create_win32_window();
    create_instance();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_render_pass();
    create_framebuffers();
    create_command_pool();
    create_command_buffers();
    create_sync_objects();
    create_descriptor_set_layout();
    create_vertex_buffer();
    create_uniform_buffer();
    create_descriptor_pool_and_set();
    create_graphics_pipeline();

    g_startTime = std::chrono::steady_clock::now();

    MSG msg{};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        update_uniform_buffer();
        draw_frame();
    }

    cleanup();
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int);

int main() {
    // Get instance handle of current module
    HINSTANCE hInstance = GetModuleHandleW(nullptr);
    return wWinMain(hInstance, nullptr, nullptr, 0);
}