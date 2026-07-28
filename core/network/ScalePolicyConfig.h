#pragma once

#include <cstddef>
#include <cstdint>

namespace Engine {
namespace Net {

enum class OverflowPolicy : uint8_t {
    LayerFirst = 0,
    ShardFirst = 1,
    LayerThenShard = 2,
};

// Configurable thresholds for in-zone layering vs cross-process sharding.
struct ScalePolicyConfig {
    // Sharding (zone instance / process).
    uint32_t shardSoftCap = 180;
    uint32_t shardHardCap = 256;
    uint32_t shardSplitThreshold = 200;
    float shardTickMsSoftLimit = 12.0f;
    float shardTickMsHardLimit = 16.0f;

    // Layering (local density within one game_server).
    uint32_t cellLayerDensityThreshold = 32;
    float densityCellSize = 32.0f;
    float layerFullRadius = 24.0f;
    float layerMidRadius = 64.0f;
    float layerFarRadius = 128.0f;
    float layerFullRadiusCrowded = 16.0f;
    size_t observersCapNormal = 64;
    size_t observersCapCrowded = 24;
    size_t maxPeerTransformsPerTick = 8192;
    size_t maxVisibilityEventsPerTick = 512;
    float spawnSpacingMin = 2.5f;
    float spawnSpacingMax = 12.0f;
    uint32_t targetMaxCellDensity = 48;
    float snapshotIntervalNormalSeconds = 0.10f;
    float snapshotIntervalCrowdedSeconds = 0.15f;
    float snapshotIntervalStressedSeconds = 0.20f;

    OverflowPolicy overflowPolicy = OverflowPolicy::LayerThenShard;

    bool ShouldRedirectLogin(uint32_t connectedPlayers, float tickP95Ms) const;
    bool IsLayeringActive(uint32_t cellDensity) const;
};

} // namespace Net
} // namespace Engine
