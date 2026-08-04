#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

struct PasswordHashRecord {
    std::vector<uint8_t> salt;
    std::vector<uint8_t> hash;
};

class PasswordHasher {
public:
#if defined(ENGINE_LOW_COST_PBKDF2)
    // Test builds only: keep credential login fast under OpenCppCoverage/CI.
    static constexpr uint32_t kDefaultIterations = 1000;
#else
    static constexpr uint32_t kDefaultIterations = 120000;
#endif

    static PasswordHashRecord HashPassword(const std::string& password,
                                           const std::vector<uint8_t>& salt = {},
                                           uint32_t iterations = kDefaultIterations);
    static bool VerifyPassword(const std::string& password, const PasswordHashRecord& record);
};

} // namespace Net
} // namespace Engine
