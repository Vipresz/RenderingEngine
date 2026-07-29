#include <gtest/gtest.h>

#include "core/network/ItemOwnershipReplication.h"

using Engine::Net::ReplicatedItemOwnership;

TEST(ItemOwnershipReplicationTest, RoundTripOwnedStacks) {
    ReplicatedItemOwnership original;
    original.ownerUserId = 42;
    original.items.push_back({"gold_coin", 100, 0});
    original.items.push_back({"quest_sword", 1, 9001});

    const Engine::Net::NetBuffer buffer = original.Serialize();
    ReplicatedItemOwnership decoded;
    ASSERT_TRUE(ReplicatedItemOwnership::Deserialize(buffer, decoded));
    EXPECT_EQ(decoded.ownerUserId, 42u);
    ASSERT_EQ(decoded.items.size(), 2u);
    EXPECT_EQ(decoded.items[0].itemId, "gold_coin");
    EXPECT_EQ(decoded.items[0].quantity, 100u);
    EXPECT_EQ(decoded.items[1].instanceId, 9001u);
}

TEST(ItemOwnershipReplicationTest, ConvertsToAndFromComponent) {
    Engine::ECS::PlayerItemOwnershipComponent component;
    component.ownerUserId = 7;
    ASSERT_TRUE(component.GrantItem("potion_health", 3));
    ASSERT_TRUE(component.GrantItem("relic_alpha", 1, 55));

    const Engine::Net::ReplicatedItemOwnership replicated =
        Engine::Net::FromOwnershipComponent(component);
    const Engine::ECS::PlayerItemOwnershipComponent roundTrip =
        Engine::Net::ToOwnershipComponent(replicated);

    EXPECT_EQ(roundTrip.ownerUserId, 7u);
    EXPECT_EQ(roundTrip.CountItem("potion_health"), 3u);
    ASSERT_EQ(roundTrip.ownedItems.size(), 2u);
    EXPECT_EQ(roundTrip.ownedItems[1].instanceId, 55u);
}
