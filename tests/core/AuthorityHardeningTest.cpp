#include <gtest/gtest.h>

#include "core/network/MovementCommand.h"
#include "core/network/NetPacket.h"
#include "core/network/NetTypes.h"

using Engine::Net::MovementCommand;
using Engine::Net::SanitizeMovementCommand;

TEST(MovementCommandSanitizeTest, ClampsDeltaAndOwnsSpeed) {
    MovementCommand command;
    command.deltaTime = 5.0f;
    command.run = true;
    command.movementSpeed = 999.0f;
    command.moveX = 4.0f;
    command.moveZ = -3.0f;

    SanitizeMovementCommand(command);

    EXPECT_FLOAT_EQ(command.deltaTime, Engine::Net::kMaxMovementDeltaSeconds);
    EXPECT_FLOAT_EQ(command.movementSpeed, Engine::Net::kAuthoritativeRunSpeed);
    EXPECT_FLOAT_EQ(command.moveX, 1.0f);
    EXPECT_FLOAT_EQ(command.moveZ, -1.0f);
}

TEST(MovementCommandSanitizeTest, WalkSpeedWhenNotRunning) {
    MovementCommand command;
    command.deltaTime = 0.016f;
    command.run = false;
    command.movementSpeed = 50.0f;
    SanitizeMovementCommand(command);
    EXPECT_FLOAT_EQ(command.movementSpeed, Engine::Net::kAuthoritativeWalkSpeed);
}

TEST(MovementCommandSanitizeTest, UsesProvidedSpeedConfig) {
    MovementCommand command;
    command.run = true;
    command.movementSpeed = 999.0f;

    Engine::Net::MovementSpeedConfig speeds;
    speeds.walkSpeed = 2.0f;
    speeds.runSpeed = 4.0f;
    SanitizeMovementCommand(command, speeds);
    EXPECT_FLOAT_EQ(command.movementSpeed, 4.0f);
}

TEST(NetPacketPayloadLimitTest, RejectsOversizedPayloadHeader) {
    Engine::Net::NetPacket packet;
    packet.header.magic = Engine::Net::kProtocolMagic;
    packet.header.version = Engine::Net::kProtocolVersion;
    packet.header.type = static_cast<Engine::Net::MessageTypeId>(Engine::Net::MessageType::GenericData);
    packet.header.payloadSize = Engine::Net::kMaxPayloadBytes + 1;
    packet.header.sequence = 1;

    // Craft a wire buffer with an oversized claim but tiny trailing bytes.
    Engine::Net::NetBuffer wire;
    wire.WriteU32(packet.header.magic);
    wire.WriteU16(packet.header.version);
    wire.WriteU16(packet.header.type);
    wire.WriteU32(packet.header.payloadSize);
    wire.WriteU64(packet.header.sequence);

    Engine::Net::NetPacket decoded;
    EXPECT_FALSE(Engine::Net::DeserializePacket(wire.Data().data(), wire.Data().size(), decoded));
}

TEST(NetPacketPayloadLimitTest, MaxPayloadConstantIsSensible) {
    EXPECT_GE(Engine::Net::kMaxPayloadBytes, 64u * 1024u);
    EXPECT_LE(Engine::Net::kMaxPayloadBytes, 1024u * 1024u);
}
