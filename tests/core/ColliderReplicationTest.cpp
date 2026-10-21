#include <gtest/gtest.h>

#include "core/network/ColliderReplication.h"

using Engine::Net::DeserializeColliderReplication;
using Engine::Net::NetBuffer;
using Engine::Net::SerializeColliderReplication;
using Engine::Physics::Components::ColliderComponent;

TEST(ColliderReplicationTest, RoundTripsBoundsFlagsAndVelocity) {
    ColliderComponent sent;
    sent.localBounds.min = glm::vec3(-1.0f, 0.0f, -2.0f);
    sent.localBounds.max = glm::vec3(1.0f, 3.0f, 2.0f);
    sent.isStatic = true;
    sent.useGravity = false;
    sent.velocity = glm::vec3(0.5f, 1.5f, -0.25f);

    ColliderComponent received;
    ASSERT_TRUE(DeserializeColliderReplication(SerializeColliderReplication(sent), received));
    EXPECT_FLOAT_EQ(received.localBounds.min.x, sent.localBounds.min.x);
    EXPECT_FLOAT_EQ(received.localBounds.max.y, sent.localBounds.max.y);
    EXPECT_FLOAT_EQ(received.localBounds.max.z, sent.localBounds.max.z);
    EXPECT_TRUE(received.isStatic);
    EXPECT_FALSE(received.useGravity);
    EXPECT_FLOAT_EQ(received.velocity.x, sent.velocity.x);
    EXPECT_FLOAT_EQ(received.velocity.y, sent.velocity.y);
    EXPECT_FLOAT_EQ(received.velocity.z, sent.velocity.z);
}

TEST(ColliderReplicationTest, TruncatedPayloadFails) {
    NetBuffer buffer;
    buffer.WriteFloat(0.0f);
    ColliderComponent received;
    EXPECT_FALSE(DeserializeColliderReplication(buffer, received));
}
