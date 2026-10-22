#include <gtest/gtest.h>

#include "core/network/WorldDropReplication.h"

using Engine::Net::IsWorldEntity;
using Engine::Net::NetBuffer;
using Engine::Net::ReplicatedWorldDrop;
using Engine::Net::kWorldEntityIdBase;

TEST(WorldDropReplicationTest, WorldEntityIdsStartAtTheReservedBase) {
    EXPECT_FALSE(IsWorldEntity(kWorldEntityIdBase - 1));
    EXPECT_TRUE(IsWorldEntity(kWorldEntityIdBase));
    EXPECT_TRUE(IsWorldEntity(kWorldEntityIdBase + 9));
}

TEST(WorldDropReplicationTest, RoundTripsItemQuantityAndInstance) {
    ReplicatedWorldDrop sent;
    sent.itemId = "ore";
    sent.quantity = 4;
    sent.instanceId = 99;

    ReplicatedWorldDrop received;
    ASSERT_TRUE(ReplicatedWorldDrop::Deserialize(sent.Serialize(), received));
    EXPECT_EQ(received.itemId, "ore");
    EXPECT_EQ(received.quantity, 4u);
    EXPECT_EQ(received.instanceId, 99u);
}

TEST(WorldDropReplicationTest, TruncatedPayloadFails) {
    NetBuffer buffer;
    buffer.WriteString("ore");
    ReplicatedWorldDrop received;
    EXPECT_FALSE(ReplicatedWorldDrop::Deserialize(buffer, received));
}
