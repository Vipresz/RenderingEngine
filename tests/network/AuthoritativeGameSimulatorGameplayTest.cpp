#include <gtest/gtest.h>

#include "core/network/InventoryAction.h"
#include "core/network/ItemCombineRecipe.h"
#include "core/network/MovementCommand.h"
#include "core/network/WorldDropReplication.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetServer.h"

using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::NetServer;

namespace {

Engine::Net::GenericDataPayload MakeInventoryActionPayload(
    const Engine::Net::InventoryActionCommand& command) {
    Engine::Net::GenericDataPayload payload;
    payload.channel = Engine::Net::kInventoryActionChannel;
    payload.data = Engine::Net::SerializeInventoryAction(command).Data();
    return payload;
}

void MovePlayerApart(AuthoritativeGameSimulator& simulator, Engine::Net::UserId userId) {
    auto* session = simulator.GetTradeSessionFor(userId);
    ASSERT_NE(session, nullptr);
    const Engine::Net::UserId partnerId =
        session->initiatorUserId == userId ? session->partnerUserId : session->initiatorUserId;

    Engine::Net::MovementCommand moveAway;
    moveAway.tick = 1;
    moveAway.moveX = 1.0f;
    moveAway.moveZ = 0.0f;
    moveAway.run = true;
    for (int i = 0; i < 200; ++i) {
        simulator.HandleMovementCommand(userId, moveAway);
        simulator.HandleMovementCommand(partnerId, moveAway);
        simulator.Update(0.05f);
    }
}

} // namespace

TEST(AuthoritativeGameSimulatorGameplayTest, DropSpawnsWorldEntityAndPickupRestoresItem) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);
    simulator.SetPickupMaxDistance(5.0f);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "player");
    simulator.Update(0.016f);
    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 5));

    Engine::Net::InventoryActionCommand dropCommand;
    dropCommand.action = Engine::Net::InventoryActionType::Drop;
    dropCommand.slotIndex = 0;
    Engine::Net::GenericDataPayload dropPayload = MakeInventoryActionPayload(dropCommand);
    ASSERT_TRUE(simulator.HandleGenericData(1, dropPayload));
    simulator.Update(0.016f);

    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 0u);
    EXPECT_EQ(simulator.GetActiveWorldDropCount(), 1u);

    const Engine::Net::NetEntityId dropId = Engine::Net::kWorldEntityIdBase;
    ASSERT_TRUE(simulator.HasWorldDrop(dropId));

    Engine::Net::InventoryActionCommand pickupCommand;
    pickupCommand.action = Engine::Net::InventoryActionType::Pickup;
    pickupCommand.worldEntityId = dropId;
    Engine::Net::GenericDataPayload pickupPayload = MakeInventoryActionPayload(pickupCommand);
    ASSERT_TRUE(simulator.HandleGenericData(1, pickupPayload));
    simulator.Update(0.016f);

    EXPECT_EQ(simulator.GetActiveWorldDropCount(), 0u);
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 5u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorGameplayTest, CombineRequiresValidRecipe) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);
    simulator.SetItemCombineRecipes({{"maple", "silver_birch", "ember_oak", 1}});

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "player");
    simulator.Update(0.016f);
    ASSERT_TRUE(simulator.GrantOwnedItem(1, "maple", 1));
    ASSERT_TRUE(simulator.GrantOwnedItem(1, "silver_birch", 1));

    Engine::Net::InventoryActionCommand combineCommand;
    combineCommand.action = Engine::Net::InventoryActionType::Combine;
    combineCommand.slotIndex = 0;
    combineCommand.secondarySlotIndex = 1;
    Engine::Net::GenericDataPayload combinePayload = MakeInventoryActionPayload(combineCommand);
    ASSERT_TRUE(simulator.HandleGenericData(1, combinePayload));
    simulator.Update(0.016f);

    EXPECT_EQ(simulator.CountOwnedItem(1, "maple"), 0u);
    EXPECT_EQ(simulator.CountOwnedItem(1, "silver_birch"), 0u);
    EXPECT_EQ(simulator.CountOwnedItem(1, "ember_oak"), 1u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorGameplayTest, RejectsSetOfferWhileLocked) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "trader_a");
    simulator.OnClientLoggedIn(2, "trader_b");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 10));
    ASSERT_TRUE(simulator.RequestTrade(1, 2));
    ASSERT_TRUE(simulator.RespondToTrade(2, 1, true));
    ASSERT_TRUE(simulator.SetTradeOffer(1, {{"gold_coin", 5, 0}}));
    ASSERT_TRUE(simulator.LockTradeOffer(1));

    EXPECT_FALSE(simulator.SetTradeOffer(1, {{"gold_coin", 3, 0}}));
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 5u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorGameplayTest, CancelsTradeWhenPlayersMoveOutOfRange) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);
    simulator.SetTradeMaxDistance(8.0f);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "trader_a");
    simulator.OnClientLoggedIn(2, "trader_b");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 10));
    ASSERT_TRUE(simulator.RequestTrade(1, 2));
    ASSERT_TRUE(simulator.RespondToTrade(2, 1, true));
    ASSERT_TRUE(simulator.SetTradeOffer(1, {{"gold_coin", 5, 0}}));

    MovePlayerApart(simulator, 1);
    simulator.SetTradeMaxDistance(2.0f);

    EXPECT_FALSE(simulator.LockTradeOffer(1));
    EXPECT_EQ(simulator.GetTradeSessionFor(1), nullptr);
    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 10u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorGameplayTest, WorldResyncChannelSendsSnapshot) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    ASSERT_TRUE(server.Start("127.0.0.1", 0, 0));
    simulator.OnClientLoggedIn(1, "player_a");
    simulator.OnClientLoggedIn(2, "player_b");
    simulator.Update(0.016f);

    Engine::Net::GenericDataPayload payload;
    payload.channel = Engine::Net::kWorldResyncChannel;
    EXPECT_TRUE(simulator.HandleGenericData(1, payload));

    server.Stop();
}

TEST(ItemCombineRecipeTest, FindsRecipeInEitherOrder) {
    const std::vector<Engine::Net::ItemCombineRecipe> recipes = {
        {"maple", "silver_birch", "ember_oak", 1},
    };
    EXPECT_TRUE(Engine::Net::FindCombineRecipe(recipes, "maple", "silver_birch").has_value());
    EXPECT_TRUE(Engine::Net::FindCombineRecipe(recipes, "silver_birch", "maple").has_value());
    EXPECT_FALSE(Engine::Net::FindCombineRecipe(recipes, "maple", "rosewood").has_value());
}

TEST(InventoryActionTest, DeserializeSupportsLegacyPayload) {
    Engine::Net::NetBuffer legacy;
    legacy.WriteU8(static_cast<uint8_t>(Engine::Net::InventoryActionType::Use));
    legacy.WriteU16(3);

    Engine::Net::InventoryActionCommand command;
    ASSERT_TRUE(Engine::Net::DeserializeInventoryAction(legacy, command));
    EXPECT_EQ(command.action, Engine::Net::InventoryActionType::Use);
    EXPECT_EQ(command.slotIndex, 3u);
    EXPECT_EQ(command.secondarySlotIndex, 0u);
    EXPECT_EQ(command.worldEntityId, 0u);
}
