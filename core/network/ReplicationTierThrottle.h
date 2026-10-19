#pragma once

#include "core/network/ReplicationTier.h"

#include <cstdint>

namespace Engine {
namespace Net {

// Distance-tier send cadence for peer transform replication.
bool ShouldSendTransformForTier(ReplicationTier tier, uint64_t tickNumber);

// Per-player ECS snapshot cadence when under crowd pressure.
float SnapshotIntervalForTier(ReplicationTier tier,
                              float normalSeconds,
                              float crowdedSeconds,
                              float stressedSeconds);

ReplicationTier MaxReplicationTier(ReplicationTier lhs, ReplicationTier rhs);

} // namespace Net
} // namespace Engine
