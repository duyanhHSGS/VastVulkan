#include <vulkan/vulkan.h>

#include <iostream>

int main() {
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

    // Cleanup
    vkDestroyInstance(instance, nullptr);
}
