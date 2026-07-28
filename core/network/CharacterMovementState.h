#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace Engine {
namespace Net {

enum class MovementAnimationState : uint8_t {
    Idle = 0,
    Walking,
    Running,
    Jumping,
    TurningLeft,
    TurningRight
};

struct CharacterMovementState {
    glm::vec3 position{0.0f};
    float yawDegrees = 0.0f;
    bool isJumping = false;
    float jumpVelocity = 0.0f;
    float groundLevel = 0.0f;
    float gravity = -9.81f;
    float jumpInitialVelocity = 5.0f;
    // Horizontal velocity captured when leaving the ground (units/sec). Frozen while airborne.
    float airVelocityX = 0.0f;
    float airVelocityZ = 0.0f;
    // True when airborne because jump was pressed (drives jump clip for the whole airtime).
    bool jumpAnimationActive = false;
    MovementAnimationState animationState = MovementAnimationState::Idle;
    // Standing RMB turn: keep the same turn clip looping across sparse mouse deltas.
    // +1 = TurningLeft, -1 = TurningRight, 0 = none.
    int8_t standingTurnSign = 0;
    float standingTurnHoldSeconds = 0.0f;
};

} // namespace Net
} // namespace Engine
