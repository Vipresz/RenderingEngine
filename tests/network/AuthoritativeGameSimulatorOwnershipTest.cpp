#include <gtest/gtest.h>

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
    ASSERT_EQ(ownership->items.size(), 1u);
    EXPECT_EQ(ownership->items[0].itemId, "gold_coin");
    EXPECT_EQ(ownership->items[0].quantity, 15u);

    server.Stop();
}
