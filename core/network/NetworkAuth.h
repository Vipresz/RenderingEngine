#pragma once

#include "core/network/SecureCompare.h"
#include "core/network/CryptoSha256.h"

#include <array>
#include <cstdint>
#include <string>

namespace Engine {
namespace Net {

inline void DigestAuthToken(const std::string& token, uint8_t outDigest[32]) {
    Sha256(reinterpret_cast<const uint8_t*>(token.data()), token.size(), outDigest);
}

// Constant-time auth token verification (length-independent via SHA-256 digest).
inline bool VerifyAuthToken(const std::string& required, const std::string& provided) {
    if (required.empty()) {
        return true;
    }

    std::array<uint8_t, 32> requiredDigest{};
    std::array<uint8_t, 32> providedDigest{};
    DigestAuthToken(required, requiredDigest.data());
    DigestAuthToken(provided, providedDigest.data());
    return SecureCompareBytes(requiredDigest.data(), providedDigest.data(), requiredDigest.size());
}

} // namespace Net
} // namespace Engine
