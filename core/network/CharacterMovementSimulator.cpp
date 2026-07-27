#include "core/network/CharacterMovementSimulator.h"

#include <algorithm>
#include <cmath>

namespace Engine {
namespace Net {

namespace {

constexpr float kGroundSnapEpsilon = 0.2f;
constexpr float kMaxStepDown = 0.35f;

void NormalizeYaw(float& yaw) {
    while (yaw > 180.0f) {
        yaw -= 360.0f;
    }
    while (yaw < -180.0f) {
        yaw += 360.0f;
    }
}

float ShortestAngleDelta(float from, float to) {
    float delta = to - from;
    while (delta > 180.0f) {
        delta -= 360.0f;
    }
    while (delta < -180.0f) {
        delta += 360.0f;
    }
    return delta;
}

void RotateYawToward(float& yawDegrees, float desiredYaw, float maxDegrees) {
    NormalizeYaw(desiredYaw);
    NormalizeYaw(yawDegrees);
    const float angleDiff = ShortestAngleDelta(yawDegrees, desiredYaw);
    if (std::abs(angleDiff) <= 0.5f) {
        yawDegrees = desiredYaw;
        return;
    }
    yawDegrees += std::copysign(std::min(std::abs(angleDiff), maxDegrees), angleDiff);
}

} // namespace

void CharacterMovementSimulator::SimulateStep(CharacterMovementState& state,
                                                const MovementCommand& command,
                                                const TerrainHeightSampler& terrainSampler) {
    const float deltaTime = std::max(command.deltaTime, 0.0f);
    glm::vec3 moveDirection(command.moveX, 0.0f, command.moveZ);
    const bool isMoving = glm::length(moveDirection) > 0.0001f;
    const float currentMovementSpeed =
        command.run ? (command.movementSpeed * 2.5f) : command.movementSpeed;

    if (isMoving) {
        moveDirection = glm::normalize(moveDirection);
    }

    const bool wasAirborne = state.isJumping;
    constexpr float kRotationSpeed = 720.0f;
    const float maxRotation = kRotationSpeed * deltaTime;

    // Grounded: steer from input. Airborne: keep leave-ground facing unless RMB look.
    if (!wasAirborne) {
        if (isMoving && command.faceMovement) {
            const float desiredYaw =
                glm::degrees(std::atan2(-moveDirection.x, -moveDirection.z)) + 180.0f;
            RotateYawToward(state.yawDegrees, desiredYaw, maxRotation);
        } else if (!command.faceMovement) {
            RotateYawToward(state.yawDegrees, command.facingYawDegrees, maxRotation);
        }
    } else if (!command.faceMovement) {
        RotateYawToward(state.yawDegrees, command.facingYawDegrees, maxRotation);
    }

    if (!wasAirborne) {
        // Track leave-ground horizontal velocity from current grounded input.
        if (isMoving) {
            state.airVelocityX = moveDirection.x * currentMovementSpeed;
            state.airVelocityZ = moveDirection.z * currentMovementSpeed;
        } else {
            state.airVelocityX = 0.0f;
            state.airVelocityZ = 0.0f;
        }
    }

    // Horizontal motion: grounded uses live airVelocity (from input); airborne freezes it.
    state.position.x += state.airVelocityX * deltaTime;
    state.position.z += state.airVelocityZ * deltaTime;

    bool startedJumpThisFrame = false;
    if (!wasAirborne) {
        const float previousY = state.position.y;
        float terrainHeight = state.groundLevel;
        if (terrainSampler) {
            terrainHeight = terrainSampler(state.position.x, state.position.z);
        }

        const bool jumpRequested =
            command.jumpPressed && std::abs(previousY - state.groundLevel) < kGroundSnapEpsilon;

        if (jumpRequested) {
            state.isJumping = true;
            state.jumpVelocity = state.jumpInitialVelocity;
            state.jumpAnimationActive = true;
            state.groundLevel = terrainHeight;
            startedJumpThisFrame = true;
        } else if (terrainSampler && (previousY - terrainHeight) > kMaxStepDown) {
            // Walked off a ledge: keep horizontal momentum, no jump clip.
            state.isJumping = true;
            state.jumpVelocity = 0.0f;
            state.jumpAnimationActive = false;
            state.groundLevel = terrainHeight;
        } else {
            state.position.y = terrainHeight;
            state.groundLevel = terrainHeight;
        }
    }

    if (state.isJumping) {
        state.jumpVelocity += state.gravity * deltaTime;
        state.position.y += state.jumpVelocity * deltaTime;

        const float terrainHeight =
            terrainSampler ? terrainSampler(state.position.x, state.position.z) : state.groundLevel;
        state.groundLevel = terrainHeight;

        if (!startedJumpThisFrame && state.position.y <= terrainHeight &&
            state.jumpVelocity <= 0.0f) {
            state.position.y = terrainHeight;
            state.isJumping = false;
            state.jumpVelocity = 0.0f;
            state.jumpAnimationActive = false;
        }
    }

    if (state.isJumping && state.jumpAnimationActive) {
        state.animationState = MovementAnimationState::Jumping;
    } else if (state.isJumping) {
        // Falling without a jump press — keep last grounded locomotion pose frozen as Idle.
        state.animationState = MovementAnimationState::Idle;
    } else if (isMoving) {
        state.animationState =
            command.run ? MovementAnimationState::Running : MovementAnimationState::Walking;
    } else {
        state.animationState = MovementAnimationState::Idle;
    }
}

float CharacterMovementSimulator::LerpAngleDegrees(float from, float to, float t) {
    float delta = to - from;
    while (delta > 180.0f) {
        delta -= 360.0f;
    }
    while (delta < -180.0f) {
        delta += 360.0f;
    }
    return from + delta * t;
}

} // namespace Net
} // namespace Engine
