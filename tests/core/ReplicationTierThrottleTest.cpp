#include <gtest/gtest.h>

#include "core/network/ReplicationTierThrottle.h"

TEST(ReplicationTierThrottleTest, TierCadence) {
    EXPECT_TRUE(Engine::Net::ShouldSendTransformForTier(Engine::Net::ReplicationTier::Full, 0));
    EXPECT_TRUE(Engine::Net::ShouldSendTransformForTier(Engine::Net::ReplicationTier::Full, 3));
    EXPECT_TRUE(Engine::Net::ShouldSendTransformForTier(Engine::Net::ReplicationTier::Mid, 0));
    EXPECT_FALSE(Engine::Net::ShouldSendTransformForTier(Engine::Net::ReplicationTier::Mid, 1));
    EXPECT_TRUE(Engine::Net::ShouldSendTransformForTier(Engine::Net::ReplicationTier::Far, 0));
    EXPECT_FALSE(Engine::Net::ShouldSendTransformForTier(Engine::Net::ReplicationTier::Far, 1));
    EXPECT_TRUE(Engine::Net::ShouldSendTransformForTier(Engine::Net::ReplicationTier::Far, 4));
}

TEST(ReplicationTierThrottleTest, SnapshotIntervalsIncreaseWithDistance) {
    const float normal = 0.1f;
    const float crowded = 0.15f;
    const float stressed = 0.2f;
    EXPECT_LT(Engine::Net::SnapshotIntervalForTier(Engine::Net::ReplicationTier::Full, normal, crowded, stressed),
              Engine::Net::SnapshotIntervalForTier(Engine::Net::ReplicationTier::Mid, normal, crowded, stressed));
    EXPECT_LT(Engine::Net::SnapshotIntervalForTier(Engine::Net::ReplicationTier::Mid, normal, crowded, stressed),
              Engine::Net::SnapshotIntervalForTier(Engine::Net::ReplicationTier::Far, normal, crowded, stressed));
}
