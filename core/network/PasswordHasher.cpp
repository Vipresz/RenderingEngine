#include "core/network/PasswordHasher.h"

#include "core/network/CryptoSha256.h"
#include "core/network/SecureCompare.h"

#include <array>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr size_t kSaltBytes = 16;
constexpr size_t kHashBytes = 32;

std::vector<uint8_t> Pbkdf2Sha256(const std::string& password,
                                  const std::vector<uint8_t>& salt,
                                  uint32_t iterations,
                                  size_t outputBytes) {
    const size_t blocks = (outputBytes + 31) / 32;
    std::vector<uint8_t> derived;
    derived.reserve(blocks * 32);

    for (size_t blockIndex = 1; blockIndex <= blocks; ++blockIndex) {
        const uint32_t blockCounter = static_cast<uint32_t>(blockIndex);
        std::array<uint8_t, 4> counterBytes = {
            static_cast<uint8_t>((blockCounter >> 24) & 0xFF),
            static_cast<uint8_t>((blockCounter >> 16) & 0xFF),
            static_cast<uint8_t>((blockCounter >> 8) & 0xFF),
            static_cast<uint8_t>(blockCounter & 0xFF),
        };

        std::vector<uint8_t> saltWithCounter;
        saltWithCounter.reserve(salt.size() + 4);
        saltWithCounter.insert(saltWithCounter.end(), salt.begin(), salt.end());
        saltWithCounter.insert(saltWithCounter.end(), counterBytes.begin(), counterBytes.end());

        uint8_t u[32];
        Engine::Net::HmacSha256(reinterpret_cast<const uint8_t*>(password.data()),
                                password.size(),
                                saltWithCounter.data(),
                                saltWithCounter.size(),
                                u);

        std::array<uint8_t, 32> block = {};
        std::memcpy(block.data(), u, 32);
        for (uint32_t i = 1; i < iterations; ++i) {
            Engine::Net::HmacSha256(reinterpret_cast<const uint8_t*>(password.data()),
                                    password.size(),
                                    u,
                                    32,
                                    u);
            for (size_t j = 0; j < 32; ++j) {
                block[j] ^= u[j];
            }
        }

        derived.insert(derived.end(), block.begin(), block.end());
    }

    derived.resize(outputBytes);
    return derived;
}

} // namespace

namespace Engine {
namespace Net {

PasswordHashRecord PasswordHasher::HashPassword(const std::string& password,
                                                const std::vector<uint8_t>& salt,
                                                uint32_t iterations) {
    PasswordHashRecord record;
    if (salt.empty()) {
        record.salt.resize(kSaltBytes);
        std::random_device rd;
        for (size_t i = 0; i < kSaltBytes; ++i) {
            record.salt[i] = static_cast<uint8_t>(rd());
        }
    } else {
        record.salt = salt;
    }

    record.hash = Pbkdf2Sha256(password, record.salt, iterations, kHashBytes);
    return record;
}

bool PasswordHasher::VerifyPassword(const std::string& password, const PasswordHashRecord& record) {
    if (record.salt.empty() || record.hash.size() != kHashBytes) {
        return false;
    }

    const std::vector<uint8_t> candidate = Pbkdf2Sha256(password, record.salt, kDefaultIterations, kHashBytes);
    if (candidate.size() != record.hash.size()) {
        return false;
    }

    return SecureCompareBytes(candidate.data(), record.hash.data(), candidate.size());
}

} // namespace Net
} // namespace Engine
