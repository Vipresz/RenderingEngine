#include "core/network/CryptoSha256.h"

#include <array>
#include <cstring>
#include <vector>

namespace {

uint32_t RotateRight(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

void Sha256Transform(const uint8_t block[64], uint32_t state[8]) {
    static const uint32_t kRoundConstants[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    uint32_t words[64];
    for (size_t i = 0; i < 16; ++i) {
        words[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (size_t i = 16; i < 64; ++i) {
        const uint32_t s0 = RotateRight(words[i - 15], 7) ^ RotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
        const uint32_t s1 = RotateRight(words[i - 2], 17) ^ RotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (size_t i = 0; i < 64; ++i) {
        const uint32_t s1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + s1 + ch + kRoundConstants[i] + words[i];
        const uint32_t s0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

} // namespace

namespace Engine {
namespace Net {

void Sha256(const uint8_t* data, size_t length, uint8_t out[32]) {
    uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    size_t offset = 0;
    while (offset + 64 <= length) {
        Sha256Transform(data + offset, state);
        offset += 64;
    }

    uint8_t block[64] = {};
    const size_t remaining = length - offset;
    std::memcpy(block, data + offset, remaining);
    block[remaining] = 0x80;

    if (remaining >= 56) {
        Sha256Transform(block, state);
        std::memset(block, 0, sizeof(block));
    }

    const uint64_t bitLength = static_cast<uint64_t>(length) * 8ULL;
    for (int i = 0; i < 8; ++i) {
        block[63 - i] = static_cast<uint8_t>((bitLength >> (8 * i)) & 0xFF);
    }
    Sha256Transform(block, state);

    for (size_t i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<uint8_t>((state[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 8) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>(state[i] & 0xFF);
    }
}

void HmacSha256(const uint8_t* key,
                size_t keyLength,
                const uint8_t* message,
                size_t messageLength,
                uint8_t out[32]) {
    std::array<uint8_t, 64> keyBlock{};
    if (keyLength > 64) {
        Sha256(key, keyLength, keyBlock.data());
    } else {
        std::memcpy(keyBlock.data(), key, keyLength);
    }

    std::array<uint8_t, 64> ipad{};
    std::array<uint8_t, 64> opad{};
    for (size_t i = 0; i < 64; ++i) {
        ipad[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x36);
        opad[i] = static_cast<uint8_t>(keyBlock[i] ^ 0x5c);
    }

    std::vector<uint8_t> inner(64 + messageLength);
    std::memcpy(inner.data(), ipad.data(), 64);
    std::memcpy(inner.data() + 64, message, messageLength);

    uint8_t innerHash[32];
    Sha256(inner.data(), inner.size(), innerHash);

    std::array<uint8_t, 96> outer{};
    std::memcpy(outer.data(), opad.data(), 64);
    std::memcpy(outer.data() + 64, innerHash, 32);
    Sha256(outer.data(), outer.size(), out);
}

} // namespace Net
} // namespace Engine
