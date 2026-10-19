#include "core/network/MigrationToken.h"

#include "core/network/CryptoSha256.h"
#include "core/network/SecureCompare.h"

#include <cstdio>
#include <sstream>
#include <vector>

namespace Engine {
namespace Net {

namespace {

std::string BytesToHex(const uint8_t* bytes, size_t length) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        out.push_back(kHex[(bytes[i] >> 4) & 0x0F]);
        out.push_back(kHex[bytes[i] & 0x0F]);
    }
    return out;
}

bool HexToBytes(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) {
        return false;
    }
    out.clear();
    out.reserve(hex.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return 10 + (c - 'a');
        }
        if (c >= 'A' && c <= 'F') {
            return 10 + (c - 'A');
        }
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int hi = nibble(hex[i]);
        const int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

std::string BuildSignedMessage(UserId userId,
                               uint32_t sourceShardId,
                               uint32_t targetShardId,
                               uint64_t expiryUnixSeconds) {
    std::ostringstream stream;
    stream << userId << ':' << sourceShardId << ':' << targetShardId << ':' << expiryUnixSeconds;
    return stream.str();
}

std::string ComputeSignature(UserId userId,
                             uint32_t sourceShardId,
                             uint32_t targetShardId,
                             uint64_t expiryUnixSeconds,
                             const std::string& authSecret) {
    const std::string message = BuildSignedMessage(userId, sourceShardId, targetShardId, expiryUnixSeconds);
    uint8_t digest[32] = {};
    HmacSha256(reinterpret_cast<const uint8_t*>(authSecret.data()),
               authSecret.size(),
               reinterpret_cast<const uint8_t*>(message.data()),
               message.size(),
               digest);
    return BytesToHex(digest, sizeof(digest));
}

} // namespace

std::string IssueMigrationToken(UserId userId,
                                uint32_t sourceShardId,
                                uint32_t targetShardId,
                                uint64_t expiryUnixSeconds,
                                const std::string& authSecret) {
    MigrationTokenPayload token;
    token.userId = userId;
    token.sourceShardId = sourceShardId;
    token.targetShardId = targetShardId;
    token.expiryUnixSeconds = expiryUnixSeconds;
    token.signatureHex = ComputeSignature(userId, sourceShardId, targetShardId, expiryUnixSeconds, authSecret);
    return SerializeMigrationToken(token);
}

bool VerifyMigrationToken(const MigrationTokenPayload& token,
                          const std::string& authSecret,
                          uint64_t nowUnixSeconds) {
    if (token.userId == kInvalidUserId || token.signatureHex.empty() || authSecret.empty()) {
        return false;
    }
    if (token.expiryUnixSeconds != 0 && nowUnixSeconds > token.expiryUnixSeconds) {
        return false;
    }
    const std::string expected =
        ComputeSignature(token.userId, token.sourceShardId, token.targetShardId, token.expiryUnixSeconds, authSecret);
    return SecureCompare(expected, token.signatureHex);
}

bool ParseMigrationToken(const std::string& tokenString, MigrationTokenPayload& out) {
    std::istringstream stream(tokenString);
    std::string userPart;
    std::string sourcePart;
    std::string targetPart;
    std::string expiryPart;
    std::string signaturePart;
    if (!std::getline(stream, userPart, ':') || !std::getline(stream, sourcePart, ':') ||
        !std::getline(stream, targetPart, ':') || !std::getline(stream, expiryPart, ':') ||
        !std::getline(stream, signaturePart)) {
        return false;
    }
    try {
        out.userId = static_cast<UserId>(std::stoul(userPart));
        out.sourceShardId = static_cast<uint32_t>(std::stoul(sourcePart));
        out.targetShardId = static_cast<uint32_t>(std::stoul(targetPart));
        out.expiryUnixSeconds = static_cast<uint64_t>(std::stoull(expiryPart));
    } catch (...) {
        return false;
    }
    out.signatureHex = signaturePart;
    return true;
}

std::string SerializeMigrationToken(const MigrationTokenPayload& token) {
    std::ostringstream stream;
    stream << token.userId << ':' << token.sourceShardId << ':' << token.targetShardId << ':'
           << token.expiryUnixSeconds << ':' << token.signatureHex;
    return stream.str();
}

} // namespace Net
} // namespace Engine
