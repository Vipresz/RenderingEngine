#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/NetPacket.h"
#include "core/network/NetTypes.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

struct LoginRequestPayload {
    UserId preferredUserId = kInvalidUserId;
    std::string displayName;
    std::string username;
    std::string password;
    std::string authToken;
    std::string migrationToken;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(preferredUserId);
        buffer.WriteString(displayName);
        buffer.WriteString(username);
        buffer.WriteString(password);
        buffer.WriteString(authToken);
        buffer.WriteString(migrationToken);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, LoginRequestPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        if (!copy.ReadU32(out.preferredUserId) || !copy.ReadString(out.displayName)) {
            return false;
        }
        if (!copy.ReadString(out.username)) {
            out.username.clear();
        }
        if (!copy.ReadString(out.password)) {
            out.password.clear();
        }
        if (!copy.ReadString(out.authToken)) {
            out.authToken.clear();
        }
        if (!copy.ReadString(out.migrationToken)) {
            out.migrationToken.clear();
        }
        return true;
    }
};

struct LoginResponsePayload {
    bool success = false;
    UserId assignedUserId = kInvalidUserId;
    std::string message;
    uint64_t udpSessionToken = 0;

    // Shard redirect (populated when this shard is at capacity).
    bool redirect = false;
    uint32_t shardId = 0;
    std::string redirectHost;
    uint16_t redirectTcpPort = 0;
    uint16_t redirectUdpPort = 0;
    std::string migrationToken;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU8(success ? 1 : 0);
        buffer.WriteU32(assignedUserId);
        buffer.WriteString(message);
        buffer.WriteU64(udpSessionToken);
        buffer.WriteU8(redirect ? 1 : 0);
        buffer.WriteU32(shardId);
        buffer.WriteString(redirectHost);
        buffer.WriteU16(redirectTcpPort);
        buffer.WriteU16(redirectUdpPort);
        buffer.WriteString(migrationToken);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, LoginResponsePayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        uint8_t successByte = 0;
        if (!copy.ReadU8(successByte) || !copy.ReadU32(out.assignedUserId) || !copy.ReadString(out.message)) {
            return false;
        }
        out.success = successByte != 0;
        if (!copy.ReadU64(out.udpSessionToken)) {
            out.udpSessionToken = 0;
        }
        out.redirect = false;
        out.shardId = 0;
        out.redirectHost.clear();
        out.redirectTcpPort = 0;
        out.redirectUdpPort = 0;
        out.migrationToken.clear();
        uint8_t redirectByte = 0;
        if (!copy.ReadU8(redirectByte)) {
            return true;
        }
        out.redirect = redirectByte != 0;
        if (!copy.ReadU32(out.shardId) || !copy.ReadString(out.redirectHost) ||
            !copy.ReadU16(out.redirectTcpPort) || !copy.ReadU16(out.redirectUdpPort)) {
            out.redirect = false;
            out.shardId = 0;
            out.redirectHost.clear();
            out.redirectTcpPort = 0;
            out.redirectUdpPort = 0;
        }
        if (!copy.ReadString(out.migrationToken)) {
            out.migrationToken.clear();
        }
        return true;
    }
};

struct TransformStatePayload {
    UserId userId = kInvalidUserId;
    uint64_t tick = 0;
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f};

    // Authoritative locomotion snapshot — required so clients can reconcile jumps
    // instead of inventing airborne state from local prediction.
    bool isJumping = false;
    bool jumpAnimationActive = false;
    float jumpVelocity = 0.0f;
    float airVelocityX = 0.0f;
    float airVelocityZ = 0.0f;
    float groundLevel = 0.0f;
    uint8_t animationState = 0; // MovementAnimationState
    float currentAnimationTime = 0.0f;
    float locomotionSpeed = 0.0f;
    bool isLooping = true;
    bool landingActive = false;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(userId);
        buffer.WriteU64(tick);
        buffer.WriteFloat(position.x);
        buffer.WriteFloat(position.y);
        buffer.WriteFloat(position.z);
        buffer.WriteFloat(rotationDegrees.x);
        buffer.WriteFloat(rotationDegrees.y);
        buffer.WriteFloat(rotationDegrees.z);
        buffer.WriteU8(isJumping ? 1 : 0);
        buffer.WriteU8(jumpAnimationActive ? 1 : 0);
        buffer.WriteFloat(jumpVelocity);
        buffer.WriteFloat(airVelocityX);
        buffer.WriteFloat(airVelocityZ);
        buffer.WriteFloat(groundLevel);
        buffer.WriteU8(animationState);
        buffer.WriteFloat(currentAnimationTime);
        buffer.WriteFloat(locomotionSpeed);
        buffer.WriteU8(isLooping ? 1 : 0);
        buffer.WriteU8(landingActive ? 1 : 0);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, TransformStatePayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        if (!copy.ReadU32(out.userId) ||
            !copy.ReadU64(out.tick) ||
            !copy.ReadFloat(out.position.x) ||
            !copy.ReadFloat(out.position.y) ||
            !copy.ReadFloat(out.position.z) ||
            !copy.ReadFloat(out.rotationDegrees.x) ||
            !copy.ReadFloat(out.rotationDegrees.y) ||
            !copy.ReadFloat(out.rotationDegrees.z)) {
            return false;
        }

        // Legacy payloads (pre-jump snapshot) remain valid; jump fields default to grounded.
        out.isJumping = false;
        out.jumpAnimationActive = false;
        out.jumpVelocity = 0.0f;
        out.airVelocityX = 0.0f;
        out.airVelocityZ = 0.0f;
        out.groundLevel = out.position.y;
        out.animationState = 0;
        out.currentAnimationTime = 0.0f;
        out.locomotionSpeed = 0.0f;
        out.isLooping = true;
        out.landingActive = false;

        uint8_t jumping = 0;
        uint8_t jumpAnim = 0;
        if (!copy.ReadU8(jumping)) {
            return true;
        }
        if (!copy.ReadU8(jumpAnim) ||
            !copy.ReadFloat(out.jumpVelocity) ||
            !copy.ReadFloat(out.airVelocityX) ||
            !copy.ReadFloat(out.airVelocityZ) ||
            !copy.ReadFloat(out.groundLevel) ||
            !copy.ReadU8(out.animationState)) {
            return false;
        }
        out.isJumping = jumping != 0;
        out.jumpAnimationActive = jumpAnim != 0;

        uint8_t looping = 1;
        uint8_t landing = 0;
        if (!copy.ReadFloat(out.currentAnimationTime) || !copy.ReadFloat(out.locomotionSpeed) ||
            !copy.ReadU8(looping) || !copy.ReadU8(landing)) {
            return true;
        }
        out.isLooping = looping != 0;
        out.landingActive = landing != 0;
        return true;
    }
};

struct PingPayload {
    uint64_t sequence = 0;
    uint64_t sentTimestampMs = 0;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU64(sequence);
        buffer.WriteU64(sentTimestampMs);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, PingPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU64(out.sequence) && copy.ReadU64(out.sentTimestampMs);
    }
};

struct PongPayload {
    uint64_t sequence = 0;
    uint64_t sentTimestampMs = 0;
    uint64_t serverTimestampMs = 0;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU64(sequence);
        buffer.WriteU64(sentTimestampMs);
        buffer.WriteU64(serverTimestampMs);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, PongPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU64(out.sequence) && copy.ReadU64(out.sentTimestampMs) &&
               copy.ReadU64(out.serverTimestampMs);
    }
};

// Extensible channel-based payload for future game data.
struct GenericDataPayload {
    UserId userId = kInvalidUserId;
    uint64_t tick = 0;
    std::string channel;
    std::vector<uint8_t> data;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(userId);
        buffer.WriteU64(tick);
        buffer.WriteString(channel);
        buffer.WriteBlob(data);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, GenericDataPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.userId) &&
               copy.ReadU64(out.tick) &&
               copy.ReadString(out.channel) &&
               copy.ReadBlob(out.data);
    }
};

struct PeerEventPayload {
    UserId userId = kInvalidUserId;
    std::string displayName;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(userId);
        buffer.WriteString(displayName);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, PeerEventPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.userId) && copy.ReadString(out.displayName);
    }
};

struct UdpSessionRegisterPayload {
    UserId userId = kInvalidUserId;
    uint64_t sessionToken = 0;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(userId);
        buffer.WriteU64(sessionToken);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, UdpSessionRegisterPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.userId) && copy.ReadU64(out.sessionToken);
    }
};

struct MovementAckPayload {
    UserId userId = kInvalidUserId;
    uint64_t tick = 0;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(userId);
        buffer.WriteU64(tick);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, MovementAckPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.userId) && copy.ReadU64(out.tick);
    }
};

struct TransformBatchPayload {
    static constexpr uint8_t kMaxTransformsPerBatch = 8;

    std::vector<TransformStatePayload> transforms;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        const uint8_t count = static_cast<uint8_t>((std::min)(transforms.size(), static_cast<size_t>(kMaxTransformsPerBatch)));
        buffer.WriteU8(count);
        for (uint8_t i = 0; i < count; ++i) {
            const NetBuffer encoded = transforms[i].Serialize();
            buffer.WriteBlob(encoded.Data());
        }
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, TransformBatchPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        uint8_t count = 0;
        if (!copy.ReadU8(count)) {
            return false;
        }
        out.transforms.clear();
        out.transforms.reserve(count);
        for (uint8_t i = 0; i < count; ++i) {
            std::vector<uint8_t> encodedBytes;
            if (!copy.ReadBlob(encodedBytes) || encodedBytes.empty()) {
                return false;
            }
            TransformStatePayload transform;
            NetBuffer encoded(std::move(encodedBytes));
            if (!TransformStatePayload::Deserialize(encoded, transform)) {
                return false;
            }
            out.transforms.push_back(std::move(transform));
        }
        return true;
    }
};

struct ZoneTransferPayload {
    UserId userId = kInvalidUserId;
    uint32_t shardId = 0;
    std::string redirectHost;
    uint16_t redirectTcpPort = 0;
    uint16_t redirectUdpPort = 0;
    std::string migrationToken;
    std::string reason;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(userId);
        buffer.WriteU32(shardId);
        buffer.WriteString(redirectHost);
        buffer.WriteU16(redirectTcpPort);
        buffer.WriteU16(redirectUdpPort);
        buffer.WriteString(migrationToken);
        buffer.WriteString(reason);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, ZoneTransferPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.userId) && copy.ReadU32(out.shardId) && copy.ReadString(out.redirectHost) &&
               copy.ReadU16(out.redirectTcpPort) && copy.ReadU16(out.redirectUdpPort) &&
               copy.ReadString(out.migrationToken) && copy.ReadString(out.reason);
    }
};

struct PopulationHeartbeatPayload {
    uint32_t shardId = 0;
    uint32_t playerCount = 0;
    uint32_t maxPlayers = 0;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(shardId);
        buffer.WriteU32(playerCount);
        buffer.WriteU32(maxPlayers);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, PopulationHeartbeatPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.shardId) && copy.ReadU32(out.playerCount) && copy.ReadU32(out.maxPlayers);
    }
};

struct PopulationQueryPayload {
    uint32_t shardId = 0;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(shardId);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, PopulationQueryPayload& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadU32(out.shardId);
    }
};

inline NetPacket MakeMessage(MessageType type, const NetBuffer& payload, uint64_t sequence = 0) {
    NetPacket message;
    message.header.type = static_cast<MessageTypeId>(type);
    message.header.payloadSize = static_cast<uint32_t>(payload.Size());
    message.header.sequence = sequence;
    message.payload = payload;
    return message;
}

} // namespace Net
} // namespace Engine
