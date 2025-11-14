#include <iostream>
#include <vulkan/vulkan.hpp>

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

int main() {
    // uint64_t a[1'000'000'000];
    // for (size_t i = 0; i < 1'000'000'000; ++i) a[i]++;

    // Create instance
    VkInstance instance{};
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    if (vkCreateInstance(&info, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!\n";
        return -1;
    }

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "No GPUs with Vulkan support found!\n";
        return -1;
    }
    VkPhysicalDevice devices[16];
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

    // Print GPU info
    for (uint32_t i = 0; i < deviceCount; ++i) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        std::cout << "GPU #" << i << ":\n";
        std::cout << "  Name: " << props.deviceName << "\n";
        std::cout << "  Type: ";
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                std::cout << "Integrated\n";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                std::cout << "Discrete\n";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                std::cout << "Virtual\n";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                std::cout << "CPU\n";
                break;
            default:
                std::cout << "Other\n";
                break;
        }
        std::cout << "  API Version: "
                  << VK_VERSION_MAJOR(props.apiVersion) << "."
                  << VK_VERSION_MINOR(props.apiVersion) << "."
                  << VK_VERSION_PATCH(props.apiVersion) << "\n";
        std::cout << "  Max Compute Units: " << props.limits.maxComputeWorkGroupCount[0] << "\n\n";
    }
    // pick the 0th device
    VkPhysicalDevice physicalDevice = devices[0];
    // queue
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    VkQueueFamilyProperties queueFamilies[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);
    std::cout << "There are " << queueFamilyCount << " work lines." << '\n';
    // get the computing work line
    uint32_t computeQueueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        std::cout << "Work line #" << i << " can do: ";

        VkQueueFlags f = queueFamilies[i].queueFlags;
        if (f & VK_QUEUE_GRAPHICS_BIT) std::cout << "Graphics ";
        if (f & VK_QUEUE_COMPUTE_BIT) {
            std::cout << "Compute ";
            computeQueueFamilyIndex = i;
        }
        if (f & VK_QUEUE_TRANSFER_BIT) std::cout << "Transfer ";
        if (f & VK_QUEUE_SPARSE_BINDING_BIT) std::cout << "Sparse ";
        if (f & VK_QUEUE_PROTECTED_BIT) std::cout << "Protected ";
        if (f & VK_QUEUE_VIDEO_DECODE_BIT_KHR) std::cout << "Decode ";
        if (f & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) std::cout << "Protected ";
        if (f & VK_QUEUE_OPTICAL_FLOW_BIT_NV) std::cout << "(What the hell is this?) ";
        if (f & VK_QUEUE_DATA_GRAPH_BIT_ARM) std::cout << "ARM magics ";
        std::cout << '\n';
    }
    if (computeQueueFamilyIndex == UINT32_MAX) {
        std::cerr << "No compute queue found!\n";
        return -1;
    }
    std::cout << "Are you still the same?" << '\n';
    // got the computing work line
    // make a robot dude
    VkDevice logicalDevice;
    float queuePriority = 1.0f;  // asap!
    // conveyor belt
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;  // structure type is DEVICE_QUEUE_CREATE
    queueInfo.queueFamilyIndex = computeQueueFamilyIndex;          // select the work line
    queueInfo.queueCount = 1;                                      // ???? huh?
    queueInfo.pQueuePriorities = &queuePriority;                   // this belt goes first now!?
    // robot dude's info
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;  // structure type is DEVICE_CREATE
    deviceInfo.queueCreateInfoCount = 1;                      // ??
    deviceInfo.pQueueCreateInfos = &queueInfo;                // stamp
    // make robot!
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
        std::cerr << "Failed to create logical device!\n";
        return -1;
    }
    std::cout << "Logical (not real) robot dude was made!!" << '\n';
    // Cleanup
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroyInstance(instance, nullptr);
}
