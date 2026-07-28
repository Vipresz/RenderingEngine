#include "core/network/LayerInstancePolicy.h"

#include <algorithm>

namespace Engine {
namespace Net {

namespace {

uint32_t LayerCapacity(const ScalePolicyConfig& policy) {
    return policy.shardHardCap > 0 ? policy.shardHardCap : 0;
}

uint32_t SplitSeedCount(const ScalePolicyConfig& policy) {
    if (policy.layerSplitSeedPlayers > 0) {
        return policy.layerSplitSeedPlayers;
    }
    const uint32_t capacity = LayerCapacity(policy);
    return capacity > 0 ? capacity / 2 : 0;
}

} // namespace

LayerLoginAdmission EvaluateLayerLoginAdmission(uint32_t localPlayers,
                                              uint32_t overflowLayerPlayers,
                                              bool hasOverflowTarget,
                                              const ScalePolicyConfig& policy) {
    const uint32_t capacity = LayerCapacity(policy);
    if (capacity == 0 || localPlayers < capacity) {
        return LayerLoginAdmission::AcceptLocal;
    }

    if (!hasOverflowTarget) {
        return LayerLoginAdmission::RejectFull;
    }

    const uint32_t seed = SplitSeedCount(policy);
    if (overflowLayerPlayers < seed) {
        // Do not send the 257th player (or any solo join) to an under-seeded overflow layer.
        return LayerLoginAdmission::SplitPending;
    }

    if (overflowLayerPlayers < capacity) {
        return LayerLoginAdmission::RedirectOverflow;
    }

    return LayerLoginAdmission::RejectFull;
}

LayerSplitPlan PlanLayerSplit(uint32_t localPlayers, const ScalePolicyConfig& policy) {
    LayerSplitPlan plan;
    const uint32_t capacity = LayerCapacity(policy);
    if (capacity == 0 || localPlayers < capacity) {
        return plan;
    }

    plan.shouldSplit = true;
    const uint32_t seed = SplitSeedCount(policy);
    const uint32_t half = localPlayers / 2;
    plan.migrateCount = seed > 0 ? (std::min)(seed, half) : half;
    if (plan.migrateCount == 0 && localPlayers > 0) {
        plan.migrateCount = 1;
    }
    return plan;
}

} // namespace Net
} // namespace Engine
