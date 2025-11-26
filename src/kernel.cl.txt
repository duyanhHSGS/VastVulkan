// =======================
//  GPU SHA-256 KERNEL
//  Fast, bitwise, unrolled
// =======================

__constant uint K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

#define ROTR(x,n) ((x >> n) | (x << (32 - n)))
#define CH(x,y,z) ((x & y) ^ (~x & z))
#define MAJ(x,y,z) ((x & y) ^ (x & z) ^ (y & z))
#define EP0(x) (ROTR(x,2) ^ ROTR(x,13) ^ ROTR(x,22))
#define EP1(x) (ROTR(x,6) ^ ROTR(x,11) ^ ROTR(x,25))
#define SIG0(x) (ROTR(x,7) ^ ROTR(x,18) ^ (x >> 3))
#define SIG1(x) (ROTR(x,17) ^ ROTR(x,19) ^ (x >> 10))

__kernel void sha256_kernel(
    __global const uchar* input,   // 64 bytes per item
    __global uchar* output         // 32 bytes per item
) {
    int id = get_global_id(0);

    // Load the 64‑byte block
    uint w[64];
    uint i;
    for (i = 0; i < 16; i++) {
        uint base = id * 64 + i * 4;
        w[i] = (input[base] << 24) |
               (input[base + 1] << 16) |
               (input[base + 2] << 8) |
               (input[base + 3]);
    }

    // Message schedule
    for (i = 16; i < 64; i++) {
        w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];
    }

    // Initial hash values
    uint a = 0x6a09e667u, b = 0xbb67ae85u, c = 0x3c6ef372u, d = 0xa54ff53au;
    uint e = 0x510e527fu, f = 0x9b05688cu, g = 0x1f83d9abu, h = 0x5be0cd19u;

    // Main loop
    for (i = 0; i < 64; i++) {
        uint t1 = h + EP1(e) + CH(e,f,g) + K[i] + w[i];
        uint t2 = EP0(a) + MAJ(a,b,c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    // Final state
    uint H[8] = {
        0x6a09e667u + a,
        0xbb67ae85u + b,
        0x3c6ef372u + c,
        0xa54ff53au + d,
        0x510e527fu + e,
        0x9b05688cu + f,
        0x1f83d9abu + g,
        0x5be0cd19u + h
    };

    // Write result
    uint base = id * 32;
    for (i = 0; i < 8; i++) {
        output[base + i*4]     = (uchar)(H[i] >> 24);
        output[base + i*4 + 1] = (uchar)(H[i] >> 16);
        output[base + i*4 + 2] = (uchar)(H[i] >> 8);
        output[base + i*4 + 3] = (uchar)(H[i]);
    }
}
