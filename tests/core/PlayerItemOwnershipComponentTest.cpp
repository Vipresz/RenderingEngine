#include <gtest/gtest.h>

#include "core/ecs/components/PlayerItemOwnershipComponent.h"

using Engine::ECS::PlayerItemOwnershipComponent;
using Engine::ECS::kDefaultBagCapacity;

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

TEST(PlayerItemOwnershipComponentTest, GrantItemRollsBackWhenBagIsFull) {
    PlayerItemOwnershipComponent ownership;
    for (uint16_t slot = 0; slot < kDefaultBagCapacity; ++slot) {
        ASSERT_TRUE(ownership.GrantItem("filler", 1, 0, 1, kDefaultBagCapacity));
    }
    EXPECT_EQ(ownership.ownedItems.size(), kDefaultBagCapacity);

    EXPECT_FALSE(ownership.GrantItem("ore", 5, 0, 20, kDefaultBagCapacity));
    EXPECT_EQ(ownership.CountItem("ore"), 0u);
    EXPECT_EQ(ownership.ownedItems.size(), kDefaultBagCapacity);
}

TEST(PlayerItemOwnershipComponentTest, RemoveFromSlotSupportsUseAndDrop) {
    PlayerItemOwnershipComponent ownership;
    ASSERT_TRUE(ownership.GrantItem("maple", 10, 0, 20, kDefaultBagCapacity));
    EXPECT_EQ(ownership.ownedItems.size(), 1u);
    EXPECT_EQ(ownership.ownedItems[0].slotIndex, 0u);

    EXPECT_TRUE(ownership.RemoveFromSlot(0, 1, false));
    EXPECT_EQ(ownership.CountItem("maple"), 9u);

    EXPECT_TRUE(ownership.RemoveFromSlot(0, 0, true));
    EXPECT_EQ(ownership.CountItem("maple"), 0u);
    EXPECT_TRUE(ownership.ownedItems.empty());
}

TEST(PlayerItemOwnershipComponentTest, TransferItemToRollsBackOnRecipientFailure) {
    PlayerItemOwnershipComponent giver;
    PlayerItemOwnershipComponent receiver;
    giver.ownerUserId = 3;
    receiver.ownerUserId = 4;
    ASSERT_TRUE(giver.GrantItem("wood", 10));

    for (uint16_t slot = 0; slot < kDefaultBagCapacity; ++slot) {
        ASSERT_TRUE(receiver.GrantItem("filler", 1, 0, 1, kDefaultBagCapacity));
    }

    const size_t giverStackCount = giver.ownedItems.size();
    const size_t receiverStackCount = receiver.ownedItems.size();

    EXPECT_FALSE(giver.TransferItemTo(receiver, "wood", 4, 20, kDefaultBagCapacity));
    EXPECT_EQ(giver.CountItem("wood"), 10u);
    EXPECT_EQ(receiver.CountItem("wood"), 0u);
    EXPECT_EQ(giver.ownedItems.size(), giverStackCount);
    EXPECT_EQ(receiver.ownedItems.size(), receiverStackCount);
}
