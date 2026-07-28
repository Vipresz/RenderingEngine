#pragma once

#include "core/network/NetworkPayloads.h"
#include "core/network/ScalePolicyConfig.h"

#include <glm/glm.hpp>

namespace Engine {
namespace Net {

enum class ReplicationTier : uint8_t {
    Full = 0,
    Mid = 1,
    Far = 2,
    Hidden = 3,
};

struct EffectiveLayerRadii {
    float fullRadius = 24.0f;
    float midRadius = 64.0f;
    float farRadius = 128.0f;
};

EffectiveLayerRadii ComputeEffectiveRadii(const ScalePolicyConfig& policy, bool layeringActive);

ReplicationTier ClassifyReplicationTier(float distanceSq, const EffectiveLayerRadii& radii);

TransformStatePayload ReduceTransformForTier(const TransformStatePayload& source, ReplicationTier tier);

} // namespace Net
} // namespace Engine
