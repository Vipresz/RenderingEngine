#include <gtest/gtest.h>

#include "core/ecs/components/PlayerItemOwnershipComponent.h"

using Engine::ECS::PlayerItemOwnershipComponent;

TEST(PlayerItemOwnershipComponentTest, StacksMatchingItemIds) {
    PlayerItemOwnershipComponent ownership;
    ownership.ownerUserId = 1;
    EXPECT_TRUE(ownership.GrantItem("iron_ore", 5));
    EXPECT_TRUE(ownership.GrantItem("iron_ore", 3));
    EXPECT_EQ(ownership.CountItem("iron_ore"), 8u);
    EXPECT_TRUE(ownership.HasItem("iron_ore", 8));
}

TEST(PlayerItemOwnershipComponentTest, TracksUniqueInstancesSeparately) {
    PlayerItemOwnershipComponent ownership;
    ownership.ownerUserId = 2;
    EXPECT_TRUE(ownership.GrantItem("legendary_ring", 1, 101));
    EXPECT_TRUE(ownership.GrantItem("legendary_ring", 1, 102));
    EXPECT_EQ(ownership.CountItem("legendary_ring"), 0u);
    EXPECT_TRUE(ownership.RemoveInstance(101));
    EXPECT_FALSE(ownership.RemoveInstance(101));
    EXPECT_TRUE(ownership.HasItem("legendary_ring", 0));
}

TEST(PlayerItemOwnershipComponentTest, TransfersBetweenOwners) {
    PlayerItemOwnershipComponent giver;
    PlayerItemOwnershipComponent receiver;
    giver.ownerUserId = 3;
    receiver.ownerUserId = 4;
    ASSERT_TRUE(giver.GrantItem("wood", 10));
    EXPECT_TRUE(giver.TransferItemTo(receiver, "wood", 4));
    EXPECT_EQ(giver.CountItem("wood"), 6u);
    EXPECT_EQ(receiver.CountItem("wood"), 4u);
}

TEST(PlayerItemOwnershipComponentTest, RejectsInvalidGrants) {
    PlayerItemOwnershipComponent ownership;
    EXPECT_FALSE(ownership.GrantItem("", 1));
    EXPECT_FALSE(ownership.GrantItem("gem", 0));
    EXPECT_FALSE(ownership.GrantItem("gem", 2, 77));
}
