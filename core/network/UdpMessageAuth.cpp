#include "core/network/UdpMessageAuth.h"

#include "core/network/CryptoSha256.h"
#include "core/network/SecureCompare.h"

#include <cstring>
#include <vector>

namespace Engine {
namespace Net {

namespace {

uint64_t AuthTagFromHmac(const uint8_t digest[32]) {
    uint64_t tag = 0;
    std::memcpy(&tag, digest, sizeof(tag));
    return tag;
}

} // namespace

uint64_t ComputeUdpAuthTag(uint64_t sessionToken,
                           const void* headerWithoutAuthTag,
                           size_t headerWithoutAuthTagSize,
                           const void* payload,
                           size_t payloadSize) {
    if (sessionToken == 0) {
        return 0;
    }

    const uint8_t keyBytes[8] = {
        static_cast<uint8_t>((sessionToken >> 56) & 0xFF),
        static_cast<uint8_t>((sessionToken >> 48) & 0xFF),
        static_cast<uint8_t>((sessionToken >> 40) & 0xFF),
        static_cast<uint8_t>((sessionToken >> 32) & 0xFF),
        static_cast<uint8_t>((sessionToken >> 24) & 0xFF),
        static_cast<uint8_t>((sessionToken >> 16) & 0xFF),
        static_cast<uint8_t>((sessionToken >> 8) & 0xFF),
        static_cast<uint8_t>(sessionToken & 0xFF),
    };

    std::vector<uint8_t> message;
    message.reserve(headerWithoutAuthTagSize + payloadSize);
    if (headerWithoutAuthTagSize > 0 && headerWithoutAuthTag) {
        const auto* headerBytes = static_cast<const uint8_t*>(headerWithoutAuthTag);
        message.insert(message.end(), headerBytes, headerBytes + headerWithoutAuthTagSize);
    }
    if (payloadSize > 0 && payload) {
        const auto* payloadBytes = static_cast<const uint8_t*>(payload);
        message.insert(message.end(), payloadBytes, payloadBytes + payloadSize);
    }

    uint8_t digest[32];
    HmacSha256(keyBytes, sizeof(keyBytes), message.data(), message.size(), digest);
    return AuthTagFromHmac(digest);
}

bool VerifyUdpAuthTag(uint64_t sessionToken,
                      uint64_t authTag,
                      const void* headerWithoutAuthTag,
                      size_t headerWithoutAuthTagSize,
                      const void* payload,
                      size_t payloadSize) {
    if (sessionToken == 0 || authTag == 0) {
        return false;
    }

    const uint64_t expected =
        ComputeUdpAuthTag(sessionToken, headerWithoutAuthTag, headerWithoutAuthTagSize, payload, payloadSize);
    return SecureCompareBytes(reinterpret_cast<const uint8_t*>(&expected),
                              reinterpret_cast<const uint8_t*>(&authTag),
                              sizeof(authTag));
}

} // namespace Net
} // namespace Engine
