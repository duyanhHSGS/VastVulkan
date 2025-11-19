#version 450

// 1) Workgroup size: 9 ninjas in X direction
layout(local_size_x = 9) in;

// 2) Storage buffer: this will be hooked to your VkBuffer at set=0, binding=0
layout(set = 0, binding = 0) buffer Data {
    uint nums[];   // NOTE: 32-bit unsigned ints
};

// 3) Constant N = 9 elements (matches your C++ buffer size)
const uint N = 9;

void main() {
    // Which ninja am I?
    uint i = gl_GlobalInvocationID.x;

    // Safety: ignore any extra ninjas
    if (i >= N) {
        return;
    }

    // Math spell: double the element
    nums[i] = nums[i] * 2u;
}