#include <gtest/gtest.h>

#include "core/network/MovementCommand.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetServer.h"

using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::NetServer;

TEST(AuthoritativeGameSimulatorOwnershipTest, GrantsAndReplicatesOwnedItems) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    server.SetClientLoggedInCallback([&](Engine::Net::UserId userId, const std::string& displayName) {
        simulator.OnClientLoggedIn(userId, displayName);
    });

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "owner");
    simulator.Update(0.016f);

    EXPECT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 25));
    EXPECT_TRUE(simulator.HasOwnedItem(1, "gold_coin", 25));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 25u);
    EXPECT_TRUE(simulator.RemoveOwnedItem(1, "gold_coin", 10));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 15u);

    const Engine::Net::ReplicatedItemOwnership* ownership = simulator.GetOwnedItems(1);
    ASSERT_NE(ownership, nullptr);
    EXPECT_EQ(ownership->ownerUserId, 1u);
    ASSERT_EQ(ownership->items.size(), 2u);
    EXPECT_EQ(ownership->items[0].itemId, "gold_coin");
    EXPECT_EQ(ownership->items[0].slotIndex, 0u);
    EXPECT_EQ(ownership->items[0].quantity, 10u);
    EXPECT_EQ(ownership->items[1].slotIndex, 1u);
    EXPECT_EQ(ownership->items[1].quantity, 5u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorOwnershipTest, GrantsStarterInventoryOnLogin) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));

    const std::vector<std::pair<std::string, uint32_t>> starterInventory = {
        {"maple", 10},
    };
    simulator.OnClientLoggedIn(1, "owner", starterInventory);
    simulator.Update(0.016f);

    EXPECT_TRUE(simulator.HasOwnedItem(1, "maple", 10));
    EXPECT_EQ(simulator.CountOwnedItem(1, "maple"), 10u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorOwnershipTest, RestoresOwnershipOnLogin) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));

    Engine::Net::ReplicatedItemOwnership restored;
    restored.ownerUserId = 1;
    Engine::Net::ReplicatedOwnedItem stack;
    stack.slotIndex = 0;
    stack.itemId = "maple";
    stack.quantity = 7;
    restored.items.push_back(stack);

    simulator.OnClientLoggedIn(1, "owner", {}, &restored);
    simulator.Update(0.016f);

    EXPECT_TRUE(simulator.HasOwnedItem(1, "maple", 7));
    EXPECT_EQ(simulator.CountOwnedItem(1, "maple"), 7u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorOwnershipTest, RejectsClientBagForgery) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "owner");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "maple", 10));
    const Engine::Net::ReplicatedItemOwnership* before = simulator.GetOwnedItems(1);
    ASSERT_NE(before, nullptr);
    ASSERT_EQ(before->items.size(), 1u);

    Engine::Net::ReplicatedItemOwnership forged;
    forged.ownerUserId = 1;
    Engine::Net::ReplicatedOwnedItem slot0;
    slot0.slotIndex = 0;
    slot0.itemId = "maple";
    slot0.quantity = 20;
    Engine::Net::ReplicatedOwnedItem slot1;
    slot1.slotIndex = 1;
    slot1.itemId = "maple";
    slot1.quantity = 20;
    forged.items.push_back(slot0);
    forged.items.push_back(slot1);

    Engine::Net::GenericDataPayload payload;
    payload.channel = Engine::Net::kItemOwnershipChannel;
    payload.data = forged.Serialize().Data();
    ASSERT_TRUE(simulator.HandleGenericData(1, payload));
    simulator.Update(0.016f);

    const Engine::Net::ReplicatedItemOwnership* after = simulator.GetOwnedItems(1);
    ASSERT_NE(after, nullptr);
    EXPECT_EQ(simulator.CountOwnedItem(1, "maple"), 10u);
    ASSERT_EQ(after->items.size(), 1u);
    EXPECT_EQ(after->items[0].quantity, 10u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorOwnershipTest, AcceptsClientSlotPermutation) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "owner");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 25));

    Engine::Net::ReplicatedItemOwnership rearranged;
    rearranged.ownerUserId = 1;
    Engine::Net::ReplicatedOwnedItem lowSlot;
    lowSlot.slotIndex = 0;
    lowSlot.itemId = "gold_coin";
    lowSlot.quantity = 5;
    Engine::Net::ReplicatedOwnedItem highSlot;
    highSlot.slotIndex = 1;
    highSlot.itemId = "gold_coin";
    highSlot.quantity = 20;
    rearranged.items.push_back(lowSlot);
    rearranged.items.push_back(highSlot);

    Engine::Net::GenericDataPayload payload;
    payload.channel = Engine::Net::kItemOwnershipChannel;
    payload.data = rearranged.Serialize().Data();
    ASSERT_TRUE(simulator.HandleGenericData(1, payload));
    simulator.Update(0.016f);

    const Engine::Net::ReplicatedItemOwnership* after = simulator.GetOwnedItems(1);
    ASSERT_NE(after, nullptr);
    ASSERT_EQ(after->items.size(), 2u);
    EXPECT_EQ(after->items[0].slotIndex, 0u);
    EXPECT_EQ(after->items[0].quantity, 5u);
    EXPECT_EQ(after->items[1].slotIndex, 1u);
    EXPECT_EQ(after->items[1].quantity, 20u);

    server.Stop();
}
