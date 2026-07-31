#include "core/network/ReplicationTierThrottle.h"

#include <algorithm>

namespace Engine {
namespace Net {

bool ShouldSendTransformForTier(ReplicationTier tier, uint64_t tickNumber) {
    switch (tier) {
        case ReplicationTier::Full:
            return true;
        case ReplicationTier::Mid:
            return (tickNumber % 2) == 0;
        case ReplicationTier::Far:
            return (tickNumber % 4) == 0;
        case ReplicationTier::Hidden:
        default:
            return false;
    }
}

float SnapshotIntervalForTier(ReplicationTier tier,
                              float normalSeconds,
                              float crowdedSeconds,
                              float stressedSeconds) {
    switch (tier) {
        case ReplicationTier::Full:
            return normalSeconds;
        case ReplicationTier::Mid:
            return crowdedSeconds;
        case ReplicationTier::Far:
            return stressedSeconds;
        case ReplicationTier::Hidden:
        default:
            return stressedSeconds * 2.0f;
    }
}

ReplicationTier MaxReplicationTier(ReplicationTier lhs, ReplicationTier rhs) {
    return static_cast<ReplicationTier>((std::max)(static_cast<uint8_t>(lhs), static_cast<uint8_t>(rhs)));
}

} // namespace Net
} // namespace Engine
