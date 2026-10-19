#pragma once

#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>

namespace Engine {
namespace Net {

struct MigrationTokenPayload {
    UserId userId = kInvalidUserId;
    uint32_t sourceShardId = 0;
    uint32_t targetShardId = 0;
    uint64_t expiryUnixSeconds = 0;
    std::string signatureHex;
};

std::string IssueMigrationToken(UserId userId,
                                uint32_t sourceShardId,
                                uint32_t targetShardId,
                                uint64_t expiryUnixSeconds,
                                const std::string& authSecret);

bool VerifyMigrationToken(const MigrationTokenPayload& token,
                          const std::string& authSecret,
                          uint64_t nowUnixSeconds);

bool ParseMigrationToken(const std::string& tokenString, MigrationTokenPayload& out);
std::string SerializeMigrationToken(const MigrationTokenPayload& token);

} // namespace Net
} // namespace Engine
