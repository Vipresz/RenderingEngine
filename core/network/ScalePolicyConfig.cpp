#include "core/network/ScalePolicyConfig.h"

namespace Engine {
namespace Net {

bool ScalePolicyConfig::ShouldRedirectLogin(uint32_t connectedPlayers, float tickP95Ms) const {
    const uint32_t hardCap = shardHardCap > 0 ? shardHardCap : 0;
    if (hardCap > 0 && connectedPlayers >= hardCap) {
        return true;
    }

    if (shardSoftCap == 0) {
        return false;
    }

    switch (overflowPolicy) {
        case OverflowPolicy::ShardFirst:
            return connectedPlayers >= shardSoftCap;
        case OverflowPolicy::LayerFirst:
            return tickP95Ms >= shardTickMsHardLimit && connectedPlayers >= shardSoftCap;
        case OverflowPolicy::LayerThenShard:
        default:
            if (connectedPlayers < shardSoftCap) {
                return false;
            }
            return tickP95Ms >= shardTickMsSoftLimit;
    }
}

bool ScalePolicyConfig::IsLayeringActive(uint32_t cellDensity) const {
    return cellLayerDensityThreshold > 0 && cellDensity >= cellLayerDensityThreshold;
}

} // namespace Net
} // namespace Engine
