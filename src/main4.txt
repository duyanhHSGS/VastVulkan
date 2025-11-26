#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

constexpr char TARGET_HEX[] = "e1e4c6f7d2f2b3d617a86a471237816250e5eca8b7a1612a4cd6881bc86a3979";
constexpr std::array<char, 62> CHARS = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
    'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
constexpr size_t MAX_LEN = 5;
constexpr size_t BATCH = 1024 * 32;

std::string loadKernel(const char* filename) {
    std::ifstream file(filename);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

void hex_to_bytes(const char* hex, unsigned char* bytes) {
    for (size_t i = 0; i < 32; i++) {
        unsigned c1 = (hex[i * 2] <= '9') ? hex[i * 2] - '0' : (hex[i * 2] | 32) - 'a' + 10;
        unsigned c2 = (hex[i * 2 + 1] <= '9') ? hex[i * 2 + 1] - '0' : (hex[i * 2 + 1] | 32) - 'a' + 10;
        bytes[i] = static_cast<unsigned char>((c1 << 4) | c2);
    }
}

// Fills a buffer of 64 bytes: [password]+0x80+[zeroes]+[bitlen, 64bit LE] (SHA-256 single-block padding)
// Input len is 1..5. Output is 64 bytes.
void pad_sha256_singleblock(const char* input, size_t len, unsigned char* out) {
    memset(out, 0, 64);
    memcpy(out, input, len);
    out[len] = 0x80;
    // bit length, LE at out[63]..out[56]
    size_t bitlen = len * 8;
    out[63] = bitlen & 0xff;
    out[62] = (bitlen >> 8) & 0xff;
}

int main() {
    // Setup OpenCL
    cl_platform_id platform;
    cl_device_id device;
    clGetPlatformIDs(1, &platform, nullptr);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, nullptr);

    // Load kernel
    std::string src = loadKernel("kernel.cl");
    const char* src_ptr = src.c_str();
    cl_int err;
    cl_program program = clCreateProgramWithSource(context, 1, &src_ptr, nullptr, &err);
    err = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        // Print build log
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL);
        std::cerr << log.data() << std::endl;
        return 1;
    }

    cl_kernel kernel = clCreateKernel(program, "sha256_kernel", nullptr);

    // Target bytes
    unsigned char target_bytes[32];
    hex_to_bytes(TARGET_HEX, target_bytes);

    // For batch processing
    std::vector<unsigned char> input_buf(BATCH * 64);
    std::vector<unsigned char> output_buf(BATCH * 32);

    // Brute force, length 1..5
    bool found = false;
    std::string result;
    for (int len = 1; len <= MAX_LEN && !found; ++len) {
        uint64_t total = 1;
        for (int i = 0; i < len; ++i) total *= CHARS.size();

        for (uint64_t base = 0; base < total && !found; base += BATCH) {
            size_t this_batch = std::min<uint64_t>(BATCH, total - base);

            // Generate and pad candidates
            for (size_t idx = 0; idx < this_batch; ++idx) {
                uint64_t n = base + idx, x = n;
                char pwd[5] = {};
                for (int j = 0; j < len; ++j) {
                    pwd[j] = CHARS[x % CHARS.size()];
                    x /= CHARS.size();
                }
                pad_sha256_singleblock(pwd, len, &input_buf[idx * 64]);
            }

            // OpenCL buffers
            cl_mem input_cl = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, this_batch * 64, input_buf.data(), NULL);
            cl_mem output_cl = clCreateBuffer(context, CL_MEM_WRITE_ONLY, this_batch * 32, NULL, NULL);

            clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_cl);
            clSetKernelArg(kernel, 1, sizeof(cl_mem), &output_cl);

            size_t global = this_batch;
            clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0, NULL, NULL);
            clFinish(queue);

            clEnqueueReadBuffer(queue, output_cl, CL_TRUE, 0, this_batch * 32, output_buf.data(), 0, NULL, NULL);

            // Compare results
            for (size_t idx = 0; idx < this_batch; ++idx) {
                if (memcmp(&output_buf[idx * 32], target_bytes, 32) == 0) {
                    // Found
                    uint64_t n = base + idx, x = n;
                    result.resize(len);
                    for (int j = 0; j < len; ++j) {
                        result[j] = CHARS[x % CHARS.size()];
                        x /= CHARS.size();
                    }
                    found = true;
                    break;
                }
            }

            clReleaseMemObject(input_cl);
            clReleaseMemObject(output_cl);
        }
    }

    if (found)
        std::cout << "FOUND: " << result << std::endl;
    else
        std::cout << "No match found.\n";

    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    return 0;
}