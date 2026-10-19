#pragma once

#include <cstdint>

namespace Engine {
namespace Platform {
namespace Networking {

struct ServerTickMetrics {
    float tickMs = 0.0f;
    uint32_t connectedPlayers = 0;
    uint32_t movementCommandsProcessed = 0;
    uint32_t dirtySnapshotsSent = 0;
    uint32_t visibilityEvents = 0;
    uint32_t movementCommandsDropped = 0;
    uint64_t tickNumber = 0;

    float tickP95Ms = 0.0f;
    float tickMsMax = 0.0f;
    uint32_t cellDensityMax = 0;
    bool layeringActive = false;
    bool shardRedirectRecommended = false;
    uint32_t peerTransformsSent = 0;
    uint32_t peerTransformsDeferred = 0;
    uint32_t replicationQueueDepth = 0;

    uint32_t sessionMovementCommandsProcessed = 0;
    uint32_t sessionMovementCommandsDropped = 0;
    uint32_t sessionDirtySnapshotsSent = 0;
    uint32_t sessionVisibilityEvents = 0;
    uint32_t sessionPeerTransformsSent = 0;
    uint32_t sessionPeerTransformsDeferred = 0;
    float sessionTickMsMax = 0.0f;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
