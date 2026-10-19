#include <gtest/gtest.h>

#include "core/network/ReplicationTier.h"

#include <glm/glm.hpp>

using Engine::Net::ComputeEffectiveRadii;
using Engine::Net::ClassifyReplicationTier;
using Engine::Net::ReduceTransformForTier;
using Engine::Net::ReplicationTier;
using Engine::Net::ScalePolicyConfig;
using Engine::Net::TransformStatePayload;

TEST(ReplicationTierTest, ShrinksFullRadiusWhenLayeringActive) {
    ScalePolicyConfig policy;
    policy.layerFullRadius = 24.0f;
    policy.layerFullRadiusCrowded = 16.0f;

    const auto normal = ComputeEffectiveRadii(policy, false);
    const auto crowded = ComputeEffectiveRadii(policy, true);

    EXPECT_FLOAT_EQ(normal.fullRadius, 24.0f);
    EXPECT_FLOAT_EQ(crowded.fullRadius, 16.0f);
}

TEST(ReplicationTierTest, ClassifiesDistanceBands) {
    ScalePolicyConfig policy;
    const auto radii = ComputeEffectiveRadii(policy, false);

    EXPECT_EQ(ClassifyReplicationTier(10.0f * 10.0f, radii), ReplicationTier::Full);
    EXPECT_EQ(ClassifyReplicationTier(40.0f * 40.0f, radii), ReplicationTier::Mid);
    EXPECT_EQ(ClassifyReplicationTier(100.0f * 100.0f, radii), ReplicationTier::Far);
    EXPECT_EQ(ClassifyReplicationTier(200.0f * 200.0f, radii), ReplicationTier::Hidden);
}

TEST(ReplicationTierTest, ReducesPayloadForMidAndFarTiers) {
    TransformStatePayload full;
    full.position = glm::vec3(1.0f, 2.0f, 3.0f);
    full.rotationDegrees = glm::vec3(4.0f, 5.0f, 6.0f);
    full.isJumping = true;
    full.airVelocityX = 2.0f;
    full.animationState = 2;

    const TransformStatePayload mid = ReduceTransformForTier(full, ReplicationTier::Mid);
    EXPECT_EQ(mid.position, full.position);
    EXPECT_FLOAT_EQ(mid.rotationDegrees.y, full.rotationDegrees.y);
    EXPECT_FALSE(mid.isJumping);
    EXPECT_EQ(mid.airVelocityX, 0.0f);
    EXPECT_EQ(mid.animationState, 0);

    const TransformStatePayload farTier = ReduceTransformForTier(full, ReplicationTier::Far);
    EXPECT_EQ(farTier.position, full.position);
    EXPECT_FALSE(farTier.isJumping);
}
