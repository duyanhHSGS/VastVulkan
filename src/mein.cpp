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
// extern "C" void* malloc(std::size_t) {
//     std::cerr << "[ALERT] malloc? How about alloc your mum?\n";
//     std::abort();
// }
// extern "C" void* calloc(size_t, size_t) {
//     std::cerr << "[ALERT] calloc? How about alloc your mum?\n";
//     std::abort();
// }
// extern "C" void* realloc(void*, size_t) {
//     std::cerr << "[ALERT] realloc? How about alloc your mum?\n";
//     std::abort();
// }
void operator delete(void*) noexcept {}
void operator delete(void*, std::size_t) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete[](void*, std::size_t) noexcept {}

uint32_t findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        bool typeOK = typeFilter & (1 << i);
        bool propsOK = (memProps.memoryTypes[i].propertyFlags & props) == props;

        if (typeOK && propsOK) return i;
    }

    std::cerr << "No suitable memory type!\n";
    std::abort();
}

int main() {
    // uint64_t a[1'000'000'000];
    // for (size_t i = 0; i < 1'000'000'000; ++i) a[i]++;
    // My custom alloc
    // VkAllocationCallbacks myAllocator{};
    // myAllocator.pUserData = nullptr;
    // myAllocator.pfnAllocation = [](void* user, size_t size, size_t alignment, VkSystemAllocationScope scope) {
    //     std::cerr << "Heap detected! Aborting\n";
    //     std::abort();
    // };
    // myAllocator.pfnReallocation = nullptr;
    // myAllocator.pfnFree = nullptr;
    // myAllocator.pfnInternalAllocation = nullptr;
    // myAllocator.pfnInternalFree = nullptr;

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
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);  // get number of device
    if (deviceCount == 0) {
        std::cerr << "No GPUs with Vulkan support found!\n";
        return -1;
    }
    constexpr uint64_t MAX_DEVICE = 16;
    VkPhysicalDevice devices[MAX_DEVICE];
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
    constexpr uint64_t MAX_WORK_LINES = 16;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    VkQueueFamilyProperties queueFamilies[MAX_WORK_LINES];
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
    // make some empty slot
    VkBuffer buffer;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;  // structure type is STRUCTURE_CREATE
    bufferInfo.size = sizeof(uint64_t) * 9;                   // 8*9 bytes, make more laterrr!!!
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;    // STORAGE buffers
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // make slot!
    if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer!\n";
        return -1;
    }
    // requre explicit memory big gorilla
    // this isnt the same as  bufferInfo.size, since rounding exists
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memReq);
    std::cout << "Are you still the same?" << '\n';
    std::cout << "GPU says: I need a box with these specs\n";
    std::cout << "  Size needed: " << memReq.size << " bytes\n";
    std::cout << "  Alignment: " << memReq.alignment << " bytes\n";
    std::cout << "  Memory types bitmask: 0x"
              << std::hex << memReq.memoryTypeBits << std::dec << "\n";
    std::cout << "Are you still the same?" << '\n';
    // alloc memory!!!!! no heap violation
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;  // must be >= mem requirements
    allocInfo.memoryTypeIndex = findMemoryType(
        physicalDevice,
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    std::cout << allocInfo.memoryTypeIndex << '\n';  // returns 1 ????????????????????
    VkDeviceMemory bufferMemory;
    // alloc!
    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory!\n";
        return -1;
    }
    // bind memory
    if (vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0) != VK_SUCCESS) {
        std::cerr << "Failed to bind memory!\n";
        return -1;
    }

    // map memory
    void* data = nullptr;  // this shared "part" will be used to compute???
    if (vkMapMemory(logicalDevice, bufferMemory, 0, memReq.size, 0, &data) != VK_SUCCESS) {
        std::cerr << "Failed to bind memory!\n";
        return -1;
    }

    // use the "shared" part! we will use 9* size of (uint64_t) = 72 bytes as served before, can i use 128 bytes (to write!!) instead?
    uint64_t* nums = (uint64_t*)(data);
    for (size_t i = 0; i < 9; ++i) {
        nums[i] = i * 10;
        std::cout << "nums[" << i << "] = " << nums[i] << '\n';
    }
    // unmap later
    vkUnmapMemory(logicalDevice, bufferMemory);

    // actual compute? i aint ready for this!!!
    // Cleanup
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroyInstance(instance, nullptr);
}
