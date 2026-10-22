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
    if (command.fly) {
        glm::vec3 moveDirection(command.moveX, command.moveY, command.moveZ);
        const bool isMoving = glm::length(moveDirection) > 0.0001f;
        constexpr float kDevFlightSpeedScale = 100.0f;
        const float currentMovementSpeed = std::max(0.0f, command.movementSpeed) * kDevFlightSpeedScale;
        if (isMoving) {
            moveDirection = glm::normalize(moveDirection);
        }
        state.position += moveDirection * currentMovementSpeed * deltaTime;
        state.isJumping = false;
        state.jumpVelocity = 0.0f;
        state.jumpAnimationActive = false;
        state.airVelocityX = isMoving ? moveDirection.x * currentMovementSpeed : 0.0f;
        state.airVelocityZ = isMoving ? moveDirection.z * currentMovementSpeed : 0.0f;

        constexpr float kRotationSpeed = 720.0f;
        const float maxRotation = kRotationSpeed * deltaTime;
        if (isMoving && command.faceMovement &&
            std::abs(moveDirection.x) + std::abs(moveDirection.z) > 0.0001f) {
            const float desiredYaw =
                glm::degrees(std::atan2(-moveDirection.x, -moveDirection.z)) + 180.0f;
            RotateYawToward(state.yawDegrees, desiredYaw, maxRotation);
        } else if (!command.faceMovement) {
            RotateYawToward(state.yawDegrees, command.facingYawDegrees, maxRotation);
        }
        state.animationState = !isMoving ? MovementAnimationState::Idle
                               : command.run ? MovementAnimationState::Running
                                             : MovementAnimationState::Walking;
        state.standingTurnSign = 0;
        state.standingTurnHoldSeconds = 0.0f;
        return;
    }

    glm::vec3 moveDirection(command.moveX, 0.0f, command.moveZ);
    const bool isMoving = glm::length(moveDirection) > 0.0001f;
    const float currentMovementSpeed = std::max(0.0f, command.movementSpeed);

    if (isMoving) {
        moveDirection = glm::normalize(moveDirection);
    }

    const bool wasAirborne = state.isJumping;
    constexpr float kRotationSpeed = 720.0f;
    const float maxRotation = kRotationSpeed * deltaTime;
    const float yawBefore = state.yawDegrees;
    float standingTurnDelta = 0.0f;

    // Grounded: steer from input. Airborne: keep leave-ground facing unless RMB look.
    if (!wasAirborne) {
        if (isMoving && command.faceMovement) {
            const float desiredYaw =
                glm::degrees(std::atan2(-moveDirection.x, -moveDirection.z)) + 180.0f;
            RotateYawToward(state.yawDegrees, desiredYaw, maxRotation);
        } else if (!command.faceMovement) {
            RotateYawToward(state.yawDegrees, command.facingYawDegrees, maxRotation);
            // Standing RMB look: positive yaw change = turn right (mouse right).
            if (!isMoving) {
                standingTurnDelta = ShortestAngleDelta(yawBefore, state.yawDegrees);
                if (std::abs(standingTurnDelta) <= 0.01f) {
                    standingTurnDelta =
                        ShortestAngleDelta(state.yawDegrees, command.facingYawDegrees);
                }
            }
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
        state.standingTurnSign = 0;
        state.standingTurnHoldSeconds = 0.0f;
    } else if (state.isJumping) {
        // Falling without a jump press — keep last grounded locomotion pose frozen as Idle.
        state.animationState = MovementAnimationState::Idle;
        state.standingTurnSign = 0;
        state.standingTurnHoldSeconds = 0.0f;
    } else if (isMoving) {
        state.animationState =
            command.run ? MovementAnimationState::Running : MovementAnimationState::Walking;
        state.standingTurnSign = 0;
        state.standingTurnHoldSeconds = 0.0f;
    } else if (!command.faceMovement) {
        // Standing RMB look. Mouse motion is bursty — without a short hold, Idle↔Turn
        // every few frames restarts Left_turn/Right_turn instead of looping.
        constexpr float kTurnEnterDegrees = 0.5f;
        constexpr float kTurnHoldSeconds = 0.18f;
        const float remainingToFace =
            ShortestAngleDelta(state.yawDegrees, command.facingYawDegrees);

        float turnSignal = standingTurnDelta;
        if (std::abs(turnSignal) <= kTurnEnterDegrees) {
            turnSignal = remainingToFace;
        }

        if (std::abs(turnSignal) > kTurnEnterDegrees) {
            // Positive yaw follows mouse-right; authored clips are opposite that sign.
            state.standingTurnSign = turnSignal > 0.0f ? 1 : -1;
            state.standingTurnHoldSeconds = kTurnHoldSeconds;
        } else {
            state.standingTurnHoldSeconds =
                std::max(0.0f, state.standingTurnHoldSeconds - deltaTime);
            if (state.standingTurnHoldSeconds <= 0.0f) {
                state.standingTurnSign = 0;
            }
        }

        if (state.standingTurnSign > 0) {
            state.animationState = MovementAnimationState::TurningLeft;
        } else if (state.standingTurnSign < 0) {
            state.animationState = MovementAnimationState::TurningRight;
        } else {
            state.animationState = MovementAnimationState::Idle;
        }
    } else {
        state.animationState = MovementAnimationState::Idle;
        state.standingTurnSign = 0;
        state.standingTurnHoldSeconds = 0.0f;
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
