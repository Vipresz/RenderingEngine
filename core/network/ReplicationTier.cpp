#include "core/network/ReplicationTier.h"

#include <algorithm>
#include <cmath>

namespace Engine {
namespace Net {

EffectiveLayerRadii ComputeEffectiveRadii(const ScalePolicyConfig& policy, bool layeringActive) {
    EffectiveLayerRadii radii;
    radii.fullRadius = layeringActive ? policy.layerFullRadiusCrowded : policy.layerFullRadius;
    radii.midRadius = policy.layerMidRadius;
    radii.farRadius = policy.layerFarRadius;
    return radii;
}

ReplicationTier ClassifyReplicationTier(float distanceSq, const EffectiveLayerRadii& radii) {
    const float fullSq = radii.fullRadius * radii.fullRadius;
    const float midSq = radii.midRadius * radii.midRadius;
    const float farSq = radii.farRadius * radii.farRadius;

    if (distanceSq <= fullSq) {
        return ReplicationTier::Full;
    }
    if (distanceSq <= midSq) {
        return ReplicationTier::Mid;
    }
    if (distanceSq <= farSq) {
        return ReplicationTier::Far;
    }
    return ReplicationTier::Hidden;
}

TransformStatePayload ReduceTransformForTier(const TransformStatePayload& source, ReplicationTier tier) {
    if (tier == ReplicationTier::Full) {
        return source;
    }

    TransformStatePayload reduced = source;
    if (tier == ReplicationTier::Mid) {
        reduced.rotationDegrees.x = 0.0f;
        reduced.rotationDegrees.z = 0.0f;
        reduced.isJumping = false;
        reduced.jumpAnimationActive = false;
        reduced.jumpVelocity = 0.0f;
        reduced.airVelocityX = 0.0f;
        reduced.airVelocityZ = 0.0f;
        reduced.animationState = 0;
        return reduced;
    }

    if (tier == ReplicationTier::Far) {
        reduced.rotationDegrees.x = 0.0f;
        reduced.rotationDegrees.z = 0.0f;
        reduced.isJumping = false;
        reduced.jumpAnimationActive = false;
        reduced.jumpVelocity = 0.0f;
        reduced.airVelocityX = 0.0f;
        reduced.airVelocityZ = 0.0f;
        reduced.groundLevel = reduced.position.y;
        reduced.animationState = 0;
        return reduced;
    }

    return reduced;
}

} // namespace Net
} // namespace Engine
