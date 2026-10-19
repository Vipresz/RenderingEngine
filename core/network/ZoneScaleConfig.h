#pragma once

#include "core/network/ScalePolicyConfig.h"
#include "core/network/ShardConfig.h"

#include <string>

namespace Engine {
namespace Net {

// Zone-level sharding + layering configuration loaded from JSON.
struct ZoneScaleConfig {
    std::string zoneId;
    ShardConfig shard;
    ScalePolicyConfig scalePolicy;

    static bool TryLoadFromFile(const std::string& path, ZoneScaleConfig& out);
    static bool LoadFromJson(const std::string& json, ZoneScaleConfig& out);
};

OverflowPolicy ParseOverflowPolicy(const std::string& value, OverflowPolicy fallback);

} // namespace Net
} // namespace Engine
