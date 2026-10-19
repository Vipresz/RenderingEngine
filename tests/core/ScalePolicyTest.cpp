#include <gtest/gtest.h>

#include "core/network/ScalePolicyConfig.h"

using Engine::Net::OverflowPolicy;
using Engine::Net::ScalePolicyConfig;

TEST(ScalePolicyTest, LayeringActivatesAtThresholdWhenShardIsLargeEnough) {
    ScalePolicyConfig policy;
    policy.layerMinShardPlayers = 256;
    policy.cellLayerDensityThreshold = 32;

    EXPECT_FALSE(policy.IsLayeringActive(255, 40));
    EXPECT_FALSE(policy.IsLayeringActive(256, 31));
    EXPECT_TRUE(policy.IsLayeringActive(256, 32));
}

TEST(ScalePolicyTest, LayerThenShardWaitsForTickPressureAtSoftCap) {
    ScalePolicyConfig policy;
    policy.shardSoftCap = 180;
    policy.shardHardCap = 256;
    policy.shardTickMsSoftLimit = 12.0f;
    policy.overflowPolicy = OverflowPolicy::LayerThenShard;

    EXPECT_FALSE(policy.ShouldRedirectLogin(180, 8.0f));
    EXPECT_TRUE(policy.ShouldRedirectLogin(180, 12.0f));
    EXPECT_FALSE(policy.ShouldRedirectLogin(100, 20.0f));
    EXPECT_TRUE(policy.ShouldRedirectLogin(256, 1.0f));
}

TEST(ScalePolicyTest, ShardFirstRedirectsAtSoftCap) {
    ScalePolicyConfig policy;
    policy.shardSoftCap = 180;
    policy.overflowPolicy = OverflowPolicy::ShardFirst;

    EXPECT_FALSE(policy.ShouldRedirectLogin(179, 0.0f));
    EXPECT_TRUE(policy.ShouldRedirectLogin(180, 0.0f));
}
