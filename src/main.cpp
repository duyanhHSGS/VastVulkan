#include <openssl/sha.h>

#include <array>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

constexpr char TARGET[] =
    "e1e4c6f7d2f2b3d617a86a471237816250e5eca8b7a1612a4cd6881bc86a3979";

constexpr std::array<char, 62> chars = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

std::atomic<bool> found(false);
std::atomic<std::string*> result_ptr(nullptr);

void sha256_hex(const char* data, size_t len, char out_hex[65]) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data), len, hash);

    constexpr char hexmap[] = "0123456789abcdef";

    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        out_hex[i * 2] = hexmap[(hash[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = hexmap[hash[i] & 0xF];
    }
    out_hex[64] = '\0';
}

void worker(int length, uint64_t start, uint64_t end) {
    char buf[5];
    char hash_hex[65];

    for (uint64_t n = start; n < end && !found.load(); n++) {
        uint64_t x = n;
        for (int i = 0; i < length; i++) {
            buf[i] = chars[x % chars.size()];
            x /= chars.size();
        }

        sha256_hex(buf, length, hash_hex);

        if (memcmp(hash_hex, TARGET, 64) == 0) {
            found.store(true);
            result_ptr.store(new std::string(buf, length));
            return;
        }
    }
}

int main() {
    const int thread_count = std::thread::hardware_concurrency();
    std::cout << "Using " << thread_count << " CPU threads\n";

    for (int length = 1; length <= 5; length++) {
        uint64_t total = 1;
        for (int i = 0; i < length; i++) total *= chars.size();

        uint64_t chunk = total / thread_count;

        std::vector<std::thread> threads;

        for (int t = 0; t < thread_count; t++) {
            uint64_t start = t * chunk;
            uint64_t end = (t == thread_count - 1) ? total : start + chunk;
            threads.emplace_back(worker, length, start, end);
        }

        for (auto& th : threads) th.join();

        if (found.load()) {
            std::cout << "FOUND: " << *result_ptr.load() << "\n";
            return 0;
        }
    }

    std::cout << "No match\n";
}
