#include <gtest/gtest.h>

#include "core/network/CharacterMovementSimulator.h"
#include "core/network/InputBuffer.h"
#include "core/network/MovementCommand.h"

using Engine::Net::CharacterMovementSimulator;
using Engine::Net::CharacterMovementState;
using Engine::Net::InputBuffer;
using Engine::Net::MovementCommand;

TEST(ClientPredictionTest, ReplaysBufferedInputsAfterAuthoritativeTick) {
    InputBuffer buffer;

    MovementCommand tick1;
    tick1.tick = 1;
    tick1.deltaTime = 1.0f;
    tick1.moveZ = -1.0f;
    tick1.movementSpeed = 1.0f;
    buffer.Push(tick1);

    MovementCommand tick2;
    tick2.tick = 2;
    tick2.deltaTime = 1.0f;
    tick2.moveZ = -1.0f;
    tick2.movementSpeed = 1.0f;
    buffer.Push(tick2);

    CharacterMovementState authoritative;
    authoritative.position = glm::vec3(0.0f, 0.0f, -1.0f);
    authoritative.groundLevel = 0.0f;

    buffer.ForEachAfter(1, [&](const MovementCommand& command) {
        CharacterMovementSimulator::SimulateStep(authoritative, command, {});
    });

    EXPECT_NEAR(authoritative.position.z, -2.0f, 0.01f);
}
