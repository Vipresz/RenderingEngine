#include <gtest/gtest.h>

#include "core/network/CharacterMovementSimulator.h"
#include "core/network/MovementCommand.h"

using Engine::Net::CharacterMovementSimulator;
using Engine::Net::CharacterMovementState;
using Engine::Net::MovementCommand;

TEST(CharacterMovementSimulatorTest, MovesForwardRelativeToInput) {
    CharacterMovementState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.groundLevel = 0.0f;

    MovementCommand command;
    command.tick = 1;
    command.deltaTime = 1.0f;
    command.moveX = 0.0f;
    command.moveZ = -1.0f;
    command.movementSpeed = 2.0f;

    CharacterMovementSimulator::SimulateStep(state, command, {});
    EXPECT_NEAR(state.position.z, -2.0f, 0.01f);
}

TEST(CharacterMovementSimulatorTest, RunningUsesHigherSpeed) {
    CharacterMovementState state;
    state.groundLevel = 0.0f;

    MovementCommand walk;
    walk.deltaTime = 1.0f;
    walk.moveZ = -1.0f;
    walk.movementSpeed = 2.0f;

    MovementCommand run = walk;
    run.run = true;
    run.movementSpeed = 5.0f;

    CharacterMovementState walkState;
    CharacterMovementState runState;
    CharacterMovementSimulator::SimulateStep(walkState, walk, {});
    CharacterMovementSimulator::SimulateStep(runState, run, {});
    EXPECT_GT(std::abs(runState.position.z), std::abs(walkState.position.z));
}

TEST(CharacterMovementSimulatorTest, CameraLookAppliesFacingYawWhileIdle) {
    CharacterMovementState state;
    state.yawDegrees = 0.0f;
    state.groundLevel = 0.0f;

    MovementCommand command;
    command.deltaTime = 1.0f;
    command.faceMovement = false;
    command.facingYawDegrees = 90.0f;

    CharacterMovementSimulator::SimulateStep(state, command, {});
    EXPECT_NEAR(state.yawDegrees, 90.0f, 0.01f);
}

TEST(CharacterMovementSimulatorTest, JumpKeepsLeaveGroundHorizontalVelocity) {
    CharacterMovementState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.groundLevel = 0.0f;
    state.jumpInitialVelocity = 5.0f;
    state.gravity = -20.0f;

    MovementCommand jump;
    jump.deltaTime = 0.05f;
    jump.moveZ = -1.0f;
    jump.movementSpeed = 2.0f;
    jump.jumpPressed = true;

    CharacterMovementSimulator::SimulateStep(state, jump, {});
    EXPECT_TRUE(state.isJumping);
    EXPECT_TRUE(state.jumpAnimationActive);
    EXPECT_EQ(state.animationState, Engine::Net::MovementAnimationState::Jumping);
    EXPECT_NEAR(state.airVelocityZ, -2.0f, 0.01f);

    const float xAfterLaunch = state.position.x;
    const float zAfterLaunch = state.position.z;

    MovementCommand coast;
    coast.deltaTime = 0.05f;
    coast.moveX = 1.0f; // Air steering ignored.
    coast.moveZ = 0.0f;
    coast.movementSpeed = 2.0f;

    CharacterMovementSimulator::SimulateStep(state, coast, {});
    EXPECT_TRUE(state.isJumping);
    EXPECT_EQ(state.animationState, Engine::Net::MovementAnimationState::Jumping);
    EXPECT_NEAR(state.airVelocityZ, -2.0f, 0.01f);
    EXPECT_NEAR(state.position.x, xAfterLaunch, 0.001f);
    EXPECT_NEAR(state.position.z, zAfterLaunch + (-2.0f * 0.05f), 0.01f);
}

TEST(CharacterMovementSimulatorTest, JumpAnimationClearsOnLanding) {
    CharacterMovementState state;
    state.position = glm::vec3(0.0f, 0.0f, 0.0f);
    state.groundLevel = 0.0f;
    state.jumpInitialVelocity = 2.0f;
    state.gravity = -40.0f;

    MovementCommand jump;
    jump.deltaTime = 0.05f;
    jump.jumpPressed = true;
    CharacterMovementSimulator::SimulateStep(state, jump, {});
    ASSERT_TRUE(state.isJumping);

    MovementCommand fall;
    fall.deltaTime = 0.05f;
    for (int i = 0; i < 40 && state.isJumping; ++i) {
        CharacterMovementSimulator::SimulateStep(state, fall, {});
    }

    EXPECT_FALSE(state.isJumping);
    EXPECT_FALSE(state.jumpAnimationActive);
    EXPECT_NEAR(state.position.y, 0.0f, 0.01f);
    EXPECT_EQ(state.animationState, Engine::Net::MovementAnimationState::Idle);
}
