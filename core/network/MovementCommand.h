#pragma once

#include "core/network/NetBuffer.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Engine {
namespace Net {

constexpr const char* kMovementCommandChannel = "movement";
constexpr const char* kLoadoutChannel = "loadout";
constexpr const char* kItemOwnershipChannel = "item_ownership";
constexpr const char* kTradeChannel = "trade";
constexpr const char* kChatChannel = "chat";

// Fallback defaults when a game/server hasn't loaded character_stats.json yet.
constexpr float kAuthoritativeWalkSpeed = 2.8f;
constexpr float kAuthoritativeRunSpeed = 5.25f;
constexpr float kMaxMovementDeltaSeconds = 0.05f;

struct MovementSpeedConfig {
    float walkSpeed = kAuthoritativeWalkSpeed;
    float runSpeed = kAuthoritativeRunSpeed;
};

inline float NormalizeDegrees(float angle) {
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle < -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

// One frame of locomotion input captured on the client for prediction and rollback replay.
struct MovementCommand {
    uint64_t tick = 0;
    float deltaTime = 0.0f;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    bool run = false;
    bool jumpPressed = false;
    // When true, yaw follows move direction. When false, yaw follows facingYawDegrees (RMB look).
    bool faceMovement = true;
    float facingYawDegrees = 0.0f;
    float movementSpeed = 1.4f;
};

inline NetBuffer SerializeMovementCommand(const MovementCommand& command) {
    NetBuffer buffer;
    buffer.WriteU64(command.tick);
    buffer.WriteFloat(command.deltaTime);
    buffer.WriteFloat(command.moveX);
    buffer.WriteFloat(command.moveZ);
    buffer.WriteU8(command.run ? 1 : 0);
    buffer.WriteU8(command.jumpPressed ? 1 : 0);
    buffer.WriteU8(command.faceMovement ? 1 : 0);
    buffer.WriteFloat(command.facingYawDegrees);
    buffer.WriteFloat(command.movementSpeed);
    return buffer;
}

inline bool DeserializeMovementCommand(const NetBuffer& buffer, MovementCommand& out) {
    NetBuffer copy = buffer;
    copy.ResetRead();
    uint8_t run = 0;
    uint8_t jump = 0;
    uint8_t face = 1;
    if (!copy.ReadU64(out.tick) || !copy.ReadFloat(out.deltaTime) || !copy.ReadFloat(out.moveX) ||
        !copy.ReadFloat(out.moveZ) || !copy.ReadU8(run) || !copy.ReadU8(jump) || !copy.ReadU8(face) ||
        !copy.ReadFloat(out.facingYawDegrees) || !copy.ReadFloat(out.movementSpeed)) {
        return false;
    }
    out.run = run != 0;
    out.jumpPressed = jump != 0;
    out.faceMovement = face != 0;
    return true;
}

// Sanitize client-provided movement fields before authoritative simulation.
inline void SanitizeMovementCommand(MovementCommand& command,
                                    const MovementSpeedConfig& speeds = MovementSpeedConfig{}) {
    if (command.deltaTime < 0.0f) {
        command.deltaTime = 0.0f;
    }
    if (command.deltaTime > kMaxMovementDeltaSeconds) {
        command.deltaTime = kMaxMovementDeltaSeconds;
    }
    const float walkSpeed = speeds.walkSpeed > 0.0f ? speeds.walkSpeed : kAuthoritativeWalkSpeed;
    const float runSpeed = speeds.runSpeed > 0.0f ? speeds.runSpeed : kAuthoritativeRunSpeed;
    command.movementSpeed = command.run ? runSpeed : walkSpeed;
    command.moveX = std::clamp(command.moveX, -1.0f, 1.0f);
    command.moveZ = std::clamp(command.moveZ, -1.0f, 1.0f);
    command.facingYawDegrees = NormalizeDegrees(command.facingYawDegrees);
}

} // namespace Net
} // namespace Engine
