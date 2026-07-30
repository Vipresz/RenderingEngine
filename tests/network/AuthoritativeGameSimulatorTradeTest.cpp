#include <algorithm>
#include <gtest/gtest.h>

#include "core/ecs/components/PlayerTradeComponent.h"
#include "core/ecs/Registry.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetServer.h"
#include "network/TradeReplicationBinding.h"

using Engine::ECS::PlayerTradeComponent;
using Engine::ECS::Registry;
using Engine::ECS::TradeOfferItem;
using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::NetServer;
using Engine::Platform::Networking::RegisterTradeReplication;
using Engine::Platform::Networking::Systems::ReplicationSystem;

TEST(TradeReplicationBindingTest, ReplicatesActiveTradeSession) {
    Registry registry;
    ReplicationSystem replication;
    RegisterTradeReplication(replication);

    const Engine::EntityID entity = registry.CreateEntity().id;
    auto& trade = registry.AddComponent<PlayerTradeComponent>(entity);
    trade.sessionId = 12;
    trade.partnerUserId = 2;
    trade.phase = Engine::ECS::TradePhase::Open;
    trade.selfOffer.push_back({"potion_health", 2, 0});

    replication.RegisterReplicatedEntity(55, entity);
    const Engine::Net::EntitySnapshot snapshot = replication.BuildEntitySnapshot(55, registry, 1);
    const auto tradeComponent = std::find_if(
        snapshot.components.begin(), snapshot.components.end(), [](const Engine::Net::ComponentSnapshot& component) {
            return component.componentId == Engine::Net::kTradeReplicationComponent;
        });
    ASSERT_NE(tradeComponent, snapshot.components.end());
}

TEST(AuthoritativeGameSimulatorTradeTest, CompletesTwoPlayerTrade) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    server.SetClientLoggedInCallback([&](Engine::Net::UserId userId, const std::string& displayName) {
        simulator.OnClientLoggedIn(userId, displayName);
    });

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "trader_a");
    simulator.OnClientLoggedIn(2, "trader_b");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 50));
    ASSERT_TRUE(simulator.GrantOwnedItem(2, "health_potion", 4));

    ASSERT_TRUE(simulator.RequestTrade(1, 2));
    ASSERT_TRUE(simulator.RespondToTrade(2, 1, true));

    ASSERT_TRUE(simulator.SetTradeOffer(1, {{"gold_coin", 10, 0}}));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 40u);
    ASSERT_TRUE(simulator.SetTradeOffer(2, {{"health_potion", 2, 0}}));
    EXPECT_EQ(simulator.CountOwnedItem(2, "health_potion"), 2u);

    ASSERT_TRUE(simulator.LockTradeOffer(1));
    ASSERT_TRUE(simulator.LockTradeOffer(2));

    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 40u);
    EXPECT_EQ(simulator.CountOwnedItem(1, "health_potion"), 2u);
    EXPECT_EQ(simulator.CountOwnedItem(2, "gold_coin"), 10u);
    EXPECT_EQ(simulator.CountOwnedItem(2, "health_potion"), 2u);
    EXPECT_EQ(simulator.GetTradeSessionFor(1), nullptr);
    EXPECT_EQ(simulator.GetTradeSessionFor(2), nullptr);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorTradeTest, RestoresEscrowedItemsOnCancel) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    server.SetClientLoggedInCallback([&](Engine::Net::UserId userId, const std::string& displayName) {
        simulator.OnClientLoggedIn(userId, displayName);
    });

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "trader_a");
    simulator.OnClientLoggedIn(2, "trader_b");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 50));
    ASSERT_TRUE(simulator.RequestTrade(1, 2));
    ASSERT_TRUE(simulator.RespondToTrade(2, 1, true));
    ASSERT_TRUE(simulator.SetTradeOffer(1, {{"gold_coin", 10, 0}}));

    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 40u);
    ASSERT_TRUE(simulator.CancelTrade(1));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 50u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorTradeTest, RestoresEscrowWhenPreferredSlotOccupiedOnCancel) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    server.SetClientLoggedInCallback([&](Engine::Net::UserId userId, const std::string& displayName) {
        simulator.OnClientLoggedIn(userId, displayName);
    });

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.SetBagCapacity(2);
    simulator.OnClientLoggedIn(1, "trader_a");
    simulator.OnClientLoggedIn(2, "trader_b");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 20));
    ASSERT_TRUE(simulator.RequestTrade(1, 2));
    ASSERT_TRUE(simulator.RespondToTrade(2, 1, true));

    const TradeOfferItem escrowedOffer{"gold_coin", 20, 0, 0};
    ASSERT_TRUE(simulator.SetTradeOffer(1, {escrowedOffer}));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 0u);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "wood", 1));
    const Engine::Net::ReplicatedItemOwnership* ownership = simulator.GetOwnedItems(1);
    ASSERT_NE(ownership, nullptr);
    ASSERT_EQ(ownership->items.size(), 1u);
    EXPECT_EQ(ownership->items[0].slotIndex, 0u);
    EXPECT_EQ(ownership->items[0].itemId, "wood");

    ASSERT_TRUE(simulator.CancelTrade(1));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 20u);
    EXPECT_EQ(simulator.CountOwnedItem(1, "wood"), 1u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorTradeTest, KeepsSessionWhenRestoreFailsOnCancel) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    server.SetClientLoggedInCallback([&](Engine::Net::UserId userId, const std::string& displayName) {
        simulator.OnClientLoggedIn(userId, displayName);
    });

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.SetBagCapacity(2);
    simulator.OnClientLoggedIn(1, "trader_a");
    simulator.OnClientLoggedIn(2, "trader_b");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 20));
    ASSERT_TRUE(simulator.GrantOwnedItem(1, "iron_ore", 5));
    ASSERT_TRUE(simulator.RequestTrade(1, 2));
    ASSERT_TRUE(simulator.RespondToTrade(2, 1, true));

    const TradeOfferItem escrowedOffer{"gold_coin", 20, 0, 0};
    ASSERT_TRUE(simulator.SetTradeOffer(1, {escrowedOffer}));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 0u);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "wood", 1));
    EXPECT_FALSE(simulator.CancelTrade(1));
    EXPECT_NE(simulator.GetTradeSessionFor(1), nullptr);
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 0u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorTradeTest, UnlocksBothPlayersWhenExchangeFails) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    server.SetClientLoggedInCallback([&](Engine::Net::UserId userId, const std::string& displayName) {
        simulator.OnClientLoggedIn(userId, displayName);
    });

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "trader_a");
    simulator.OnClientLoggedIn(2, "trader_b");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(2, "gold_coin", 5));
    ASSERT_TRUE(simulator.GrantOwnedItem(2, "wood", 3));
    ASSERT_TRUE(simulator.GrantOwnedItem(1, "iron_sword", 1, 7001));
    simulator.SetBagCapacity(1);

    ASSERT_TRUE(simulator.RequestTrade(1, 2));
    ASSERT_TRUE(simulator.RespondToTrade(2, 1, true));
    ASSERT_TRUE(simulator.SetTradeOffer(1, {}));
    ASSERT_TRUE(simulator.SetTradeOffer(2, {{"gold_coin", 5, 0}, {"wood", 3, 0}}));

    ASSERT_TRUE(simulator.LockTradeOffer(1));
    ASSERT_TRUE(simulator.LockTradeOffer(2));

    const Engine::Net::TradeSession* session = simulator.GetTradeSessionFor(1);
    ASSERT_NE(session, nullptr);
    EXPECT_FALSE(session->BothLocked());
    EXPECT_EQ(simulator.CountOwnedItem(2, "gold_coin"), 0u);
    EXPECT_EQ(simulator.CountOwnedItem(2, "wood"), 0u);

    server.Stop();
}
