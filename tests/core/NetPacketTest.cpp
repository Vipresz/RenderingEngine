#include <gtest/gtest.h>

#include "core/network/NetPacket.h"
#include "core/network/NetworkPayloads.h"

using Engine::Net::MakeMessage;
using Engine::Net::MessageType;
using Engine::Net::NetPacket;
using Engine::Net::DeserializePacket;
using Engine::Net::SerializePacket;
using Engine::Net::TransformStatePayload;

TEST(NetPacketTest, SerializeDeserializeRoundTrip) {
    TransformStatePayload payload;
    payload.userId = 3;
    payload.tick = 42;
    payload.position = glm::vec3(1.0f, 2.0f, 3.0f);

    const NetPacket original = MakeMessage(MessageType::TransformState, payload.Serialize(), 100);
    const auto bytes = SerializePacket(original);

    NetPacket decoded;
    ASSERT_TRUE(DeserializePacket(bytes.Data().data(), bytes.Size(), decoded));
    EXPECT_EQ(decoded.GetType(), MessageType::TransformState);
    EXPECT_EQ(decoded.header.sequence, 100u);

    TransformStatePayload decodedPayload;
    ASSERT_TRUE(TransformStatePayload::Deserialize(decoded.payload, decodedPayload));
    EXPECT_EQ(decodedPayload.userId, 3u);
    EXPECT_EQ(decodedPayload.tick, 42u);
}

TEST(NetPacketTest, RejectsInvalidMagic) {
    NetPacket packet;
    packet.header.magic = 0;
    const auto bytes = SerializePacket(packet);

    NetPacket decoded;
    EXPECT_FALSE(DeserializePacket(bytes.Data().data(), bytes.Size(), decoded));
}
