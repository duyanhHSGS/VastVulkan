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
    bufferInfo.size = sizeof(uint32_t) * 9;                   // 4*9 bytes, make more laterrr!!!
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

    // use the "shared" part! we will use 9* size of (uint32_t) = 36 bytes as served before, can i use 128 bytes (to write!!) instead?
    uint32_t* nums = (uint32_t*)(data);
    for (size_t i = 0; i < 9; ++i) {
        nums[i] = i * 10;
        std::cout << "nums[" << i << "] = " << nums[i] << '\n';
    }
    // unmap later
    vkUnmapMemory(logicalDevice, bufferMemory);

    // ===== STEP 4–8: setup compute pipeline + dispatch =====

    // 1) Descriptor set layout: one storage buffer at set=0, binding=0
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout!\n";
        return -1;
    }

    // 2) Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool!\n";
        return -1;
    }

    // 3) Allocate descriptor set
    VkDescriptorSetAllocateInfo allocDSInfo{};
    allocDSInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocDSInfo.descriptorPool = descriptorPool;
    allocDSInfo.descriptorSetCount = 1;
    allocDSInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(logicalDevice, &allocDSInfo, &descriptorSet) != VK_SUCCESS) {
        std::cerr << "Failed to allocate descriptor set!\n";
        return -1;
    }

    // 4) Update descriptor set to point to your buffer
    VkDescriptorBufferInfo bufferDescInfo{};
    bufferDescInfo.buffer = buffer;
    bufferDescInfo.offset = 0;
    bufferDescInfo.range = VK_WHOLE_SIZE;  // or sizeof(uint32_t)*9

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferDescInfo;

    vkUpdateDescriptorSets(logicalDevice, 1, &write, 0, nullptr);

    // 5) Load shader module from SPIR-V file "ninjaclan.comp.spv"
    // NOTE: this part uses FILE* and a fixed-size buffer to avoid heap
    FILE* f = fopen("ninjaclan.comp.spv", "rb");
    if (!f) {
        std::cerr << "Failed to open ninjaclan.comp.spv!\n";
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0 || fileSize > 4096) {
        std::cerr << "Shader file size invalid or too big!\n";
        fclose(f);
        return -1;
    }

    // SPIR-V is uint32_t words; make static buffer
    uint32_t spirvData[4096 / 4];  // 4096 bytes max → 1024 uint32_t
    size_t readBytes = fread(spirvData, 1, fileSize, f);
    fclose(f);
    if (readBytes != static_cast<size_t>(fileSize)) {
        std::cerr << "Failed to read full shader file!\n";
        return -1;
    }

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = static_cast<size_t>(fileSize);
    shaderInfo.pCode = spirvData;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(logicalDevice, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module!\n";
        return -1;
    }

    // 6) Create pipeline layout (connects descriptor sets to pipeline)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout!\n";
        return -1;
    }

    // 7) Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = pipelineLayout;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline!\n";
        return -1;
    }

    // 8) Command pool & command buffer
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = computeQueueFamilyIndex;
    cmdPoolInfo.flags = 0;

    VkCommandPool commandPool;
    if (vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool!\n";
        return -1;
    }

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(logicalDevice, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffer!\n";
        return -1;
    }

    // 9) Record command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin command buffer!\n";
        return -1;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipelineLayout,
        0,  // firstSet
        1, &descriptorSet,
        0, nullptr  // dynamic offsets
    );

    // Dispatch 1 workgroup of 9 ninjas (because local_size_x = 9)
    vkCmdDispatch(commandBuffer, 1, 1, 1);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to record command buffer!\n";
        return -1;
    }

    // 10) Submit to queue and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        std::cerr << "Failed to create fence!\n";
        return -1;
    }

    if (vkQueueSubmit(computeQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        std::cerr << "Failed to submit to compute queue!\n";
        return -1;
    }

    vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);

    // 11) Map memory again and read back results
    if (vkMapMemory(logicalDevice, bufferMemory, 0, memReq.size, 0, &data) != VK_SUCCESS) {
        std::cerr << "Failed to remap memory!\n";
        return -1;
    }

    nums = (uint32_t*)data;
    std::cout << "After compute:\n";
    for (size_t i = 0; i < 9; ++i) {
        std::cout << "nums[" << i << "] = " << nums[i] << '\n';
    }
    vkUnmapMemory(logicalDevice, bufferMemory);

    // ===== Cleanup =====
    vkDestroyFence(logicalDevice, fence, nullptr);
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
    vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

    vkFreeMemory(logicalDevice, bufferMemory, nullptr);
    vkDestroyBuffer(logicalDevice, buffer, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroyInstance(instance, nullptr);
}
