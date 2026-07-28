#pragma once

#include <cstddef>
#include <cstdint>

namespace Engine {
namespace Net {

// HMAC-SHA256 auth tag for UDP datagrams (anti-spoofing; not encryption).
// When sessionToken is 0, authTag is 0 (legacy/dev — rejected by strict servers).
uint64_t ComputeUdpAuthTag(uint64_t sessionToken,
                           const void* headerWithoutAuthTag,
                           size_t headerWithoutAuthTagSize,
                           const void* payload,
                           size_t payloadSize);

bool VerifyUdpAuthTag(uint64_t sessionToken,
                      uint64_t authTag,
                      const void* headerWithoutAuthTag,
                      size_t headerWithoutAuthTagSize,
                      const void* payload,
                      size_t payloadSize);

} // namespace Net
} // namespace Engine
