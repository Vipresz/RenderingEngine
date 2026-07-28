#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/NetPacket.h"
#include "core/network/NetTypes.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {
namespace Net {

struct LoginRequestPayload {
    UserId preferredUserId = kInvalidUserId;
    std::string displayName;
    std::string username;
    std::string password;
    std::string authToken;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(preferredUserId);
        buffer.WriteString(displayName);
        buffer.WriteString(username);
        buffer.WriteString(password);
        buffer.WriteString(authToken);
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
        return true;
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
