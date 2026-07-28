#pragma once

#include "core/network/ScalePolicyConfig.h"

#include <cstdint>

namespace Engine {
namespace Net {

enum class LayerLoginAdmission : uint8_t {
    AcceptLocal = 0,
    RedirectOverflow = 1,
    SplitPending = 2,
    RejectFull = 3,
};

struct LayerSplitPlan {
    bool shouldSplit = false;
    uint32_t migrateCount = 0;
};

// Login routing for zone instance layers (256-cap shards), distinct from replication layering.
LayerLoginAdmission EvaluateLayerLoginAdmission(uint32_t localPlayers,
                                              uint32_t overflowLayerPlayers,
                                              bool hasOverflowTarget,
                                              const ScalePolicyConfig& policy);

// Combines soft-cap/tick-pressure redirect (ShouldRedirectLogin) with hard-cap layer admission.
LayerLoginAdmission EvaluateLoginAdmission(uint32_t localPlayers,
                                           uint32_t overflowLayerPlayers,
                                           bool hasOverflowTarget,
                                           const ScalePolicyConfig& policy,
                                           float tickP95Ms);

LayerSplitPlan PlanLayerSplit(uint32_t localPlayers, const ScalePolicyConfig& policy);

} // namespace Net
} // namespace Engine
