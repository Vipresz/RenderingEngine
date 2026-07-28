#include <gtest/gtest.h>

#include "core/network/LayerInstancePolicy.h"
#include "core/network/ScalePolicyConfig.h"

using Engine::Net::LayerLoginAdmission;
using Engine::Net::ScalePolicyConfig;

namespace {

ScalePolicyConfig MakeLayerPolicy() {
    ScalePolicyConfig policy;
    policy.shardHardCap = 256;
    policy.layerSplitSeedPlayers = 128;
    return policy;
}

} // namespace

TEST(LayerInstancePolicyTest, AcceptsLoginsBelowLayerCapacity) {
    const ScalePolicyConfig policy = MakeLayerPolicy();
    EXPECT_EQ(Engine::Net::EvaluateLayerLoginAdmission(255, 0, true, policy),
              LayerLoginAdmission::AcceptLocal);
}

TEST(LayerInstancePolicyTest, SplitPendingWhenOverflowLayerIsUnderSeeded) {
    const ScalePolicyConfig policy = MakeLayerPolicy();
    EXPECT_EQ(Engine::Net::EvaluateLayerLoginAdmission(256, 0, true, policy),
              LayerLoginAdmission::SplitPending);
    EXPECT_EQ(Engine::Net::EvaluateLayerLoginAdmission(256, 127, true, policy),
              LayerLoginAdmission::SplitPending);
}

TEST(LayerInstancePolicyTest, RedirectsOnceOverflowLayerHasSeedPopulation) {
    const ScalePolicyConfig policy = MakeLayerPolicy();
    EXPECT_EQ(Engine::Net::EvaluateLayerLoginAdmission(256, 128, true, policy),
              LayerLoginAdmission::RedirectOverflow);
    EXPECT_EQ(Engine::Net::EvaluateLayerLoginAdmission(256, 200, true, policy),
              LayerLoginAdmission::RedirectOverflow);
}

TEST(LayerInstancePolicyTest, PlansHalfMigrationAtCapacity) {
    const ScalePolicyConfig policy = MakeLayerPolicy();
    const auto plan = Engine::Net::PlanLayerSplit(256, policy);
    EXPECT_TRUE(plan.shouldSplit);
    EXPECT_EQ(plan.migrateCount, 128u);
}

TEST(LayerInstancePolicyTest, RejectsWhenOverflowLayerIsAlsoFull) {
    const ScalePolicyConfig policy = MakeLayerPolicy();
    EXPECT_EQ(Engine::Net::EvaluateLayerLoginAdmission(256, 256, true, policy),
              LayerLoginAdmission::RejectFull);
}
