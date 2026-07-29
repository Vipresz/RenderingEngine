#include <gtest/gtest.h>

#include "core/network/ItemOwnershipReplication.h"

using Engine::Net::ReplicatedItemOwnership;

TEST(ItemOwnershipReplicationTest, RoundTripOwnedStacks) {
    ReplicatedItemOwnership original;
    original.ownerUserId = 42;
    Engine::Net::ReplicatedOwnedItem gold;
    gold.slotIndex = 0;
    gold.itemId = "gold_coin";
    gold.quantity = 100;
    Engine::Net::ReplicatedOwnedItem sword;
    sword.slotIndex = 1;
    sword.itemId = "quest_sword";
    sword.quantity = 1;
    sword.instanceId = 9001;
    original.items.push_back(gold);
    original.items.push_back(sword);

    const Engine::Net::NetBuffer buffer = original.Serialize();
    ReplicatedItemOwnership decoded;
    ASSERT_TRUE(ReplicatedItemOwnership::Deserialize(buffer, decoded));
    EXPECT_EQ(decoded.ownerUserId, 42u);
    ASSERT_EQ(decoded.items.size(), 2u);
    EXPECT_EQ(decoded.items[0].itemId, "gold_coin");
    EXPECT_EQ(decoded.items[0].slotIndex, 0u);
    EXPECT_EQ(decoded.items[0].quantity, 100u);
    EXPECT_EQ(decoded.items[1].instanceId, 9001u);
    EXPECT_EQ(decoded.items[1].slotIndex, 1u);
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
