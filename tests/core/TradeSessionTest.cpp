#include <gtest/gtest.h>

#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/network/TradeSession.h"

using Engine::ECS::PlayerItemOwnershipComponent;
using Engine::Net::TradeOfferItem;

TEST(TradeSessionTest, ExecutesAtomicExchange) {
    PlayerItemOwnershipComponent alice;
    PlayerItemOwnershipComponent bob;
    alice.ownerUserId = 1;
    bob.ownerUserId = 2;
    ASSERT_TRUE(alice.GrantItem("gold_coin", 100));
    ASSERT_TRUE(bob.GrantItem("health_potion", 3));
    ASSERT_TRUE(bob.GrantItem("rare_gem", 1, 9001));

    const std::vector<TradeOfferItem> aliceOffer = {{"gold_coin", 40, 0}};
    const std::vector<TradeOfferItem> bobOffer = {{"health_potion", 2, 0}, {"rare_gem", 1, 9001}};

    ASSERT_TRUE(Engine::Net::ExecuteTradeExchange(alice, bob, aliceOffer, bobOffer));
    EXPECT_EQ(alice.CountItem("gold_coin"), 60u);
    EXPECT_EQ(alice.CountItem("health_potion"), 2u);
    EXPECT_TRUE(alice.RemoveInstance(9001));
    EXPECT_EQ(bob.CountItem("gold_coin"), 40u);
    EXPECT_EQ(bob.CountItem("health_potion"), 1u);
}

TEST(TradeSessionTest, RejectsOfferPlayerCannotAfford) {
    PlayerItemOwnershipComponent alice;
    PlayerItemOwnershipComponent bob;
    alice.ownerUserId = 1;
    bob.ownerUserId = 2;
    ASSERT_TRUE(alice.GrantItem("wood", 5));

    const std::vector<TradeOfferItem> aliceOffer = {{"wood", 10, 0}};
    EXPECT_FALSE(Engine::Net::ExecuteTradeExchange(alice, bob, aliceOffer, {}));
    EXPECT_EQ(alice.CountItem("wood"), 5u);
}

TEST(TradeSessionTest, BuildsPerPlayerTradeView) {
    Engine::Net::TradeSession session;
    session.sessionId = 99;
    session.initiatorUserId = 1;
    session.partnerUserId = 2;
    session.phase = Engine::ECS::TradePhase::Open;
    session.initiatorOffer = {{"gold_coin", 5, 0}};
    session.partnerOffer = {{"arrow", 20, 0}};
    session.initiatorLocked = true;

    const Engine::ECS::PlayerTradeComponent partnerView = Engine::Net::BuildTradeView(session, 2);
    EXPECT_EQ(partnerView.partnerUserId, 1u);
    EXPECT_EQ(partnerView.selfOffer.size(), 1u);
    EXPECT_EQ(partnerView.partnerOffer.size(), 1u);
    EXPECT_EQ(partnerView.selfOffer[0].itemId, "arrow");
    EXPECT_EQ(partnerView.partnerOffer[0].itemId, "gold_coin");
    EXPECT_TRUE(partnerView.partnerLocked);
}

TEST(TradeSessionTest, SerializesTradeCommands) {
    Engine::Net::TradeCommand original;
    original.action = Engine::Net::TradeAction::SetOffer;
    original.targetUserId = 42;
    original.offer.push_back({"sword", 1, 77});

    Engine::Net::TradeCommand decoded;
    ASSERT_TRUE(Engine::Net::DeserializeTradeCommand(Engine::Net::SerializeTradeCommand(original), decoded));
    EXPECT_EQ(decoded.action, Engine::Net::TradeAction::SetOffer);
    EXPECT_EQ(decoded.targetUserId, 42u);
    ASSERT_EQ(decoded.offer.size(), 1u);
    EXPECT_EQ(decoded.offer[0].instanceId, 77u);
}
