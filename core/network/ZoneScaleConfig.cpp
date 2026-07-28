#include "core/network/ZoneScaleConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace Engine {
namespace Net {
namespace {

std::string Trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string StripQuotes(std::string value) {
    value = Trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

const char* FindJsonKey(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return nullptr;
    }
    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return nullptr;
    }
    return json.c_str() + colonPos + 1;
}

bool ReadJsonString(const std::string& json, const std::string& key, std::string& outValue) {
    const char* cursor = FindJsonKey(json, key);
    if (!cursor) {
        return false;
    }
    while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor != '"') {
        return false;
    }
    ++cursor;
    std::string value;
    while (*cursor && *cursor != '"') {
        value.push_back(*cursor++);
    }
    outValue = std::move(value);
    return true;
}

bool ReadJsonUInt(const std::string& json, const std::string& key, uint32_t& outValue) {
    const char* cursor = FindJsonKey(json, key);
    if (!cursor) {
        return false;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(cursor, &end, 10);
    if (cursor == end) {
        return false;
    }
    outValue = static_cast<uint32_t>(parsed);
    return true;
}

bool ReadJsonSizeT(const std::string& json, const std::string& key, size_t& outValue) {
    uint32_t temp = 0;
    if (!ReadJsonUInt(json, key, temp)) {
        return false;
    }
    outValue = static_cast<size_t>(temp);
    return true;
}

bool ReadJsonFloat(const std::string& json, const std::string& key, float& outValue) {
    const char* cursor = FindJsonKey(json, key);
    if (!cursor) {
        return false;
    }
    char* end = nullptr;
    const float parsed = std::strtof(cursor, &end);
    if (cursor == end) {
        return false;
    }
    outValue = parsed;
    return true;
}

bool ReadJsonUInt16(const std::string& json, const std::string& key, uint16_t& outValue) {
    uint32_t temp = 0;
    if (!ReadJsonUInt(json, key, temp)) {
        return false;
    }
    outValue = static_cast<uint16_t>(temp);
    return true;
}

std::string ExtractJsonObject(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return {};
    }
    const size_t openBrace = json.find('{', keyPos + needle.size());
    if (openBrace == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (size_t i = openBrace; i < json.size(); ++i) {
        if (json[i] == '{') {
            ++depth;
        } else if (json[i] == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(openBrace, i - openBrace + 1);
            }
        }
    }
    return {};
}

void ApplyShardJson(const std::string& shardJson, ShardConfig& shard) {
    ReadJsonUInt(shardJson, "shardId", shard.shardId);
    ReadJsonUInt(shardJson, "maxPlayers", shard.maxPlayers);
    ReadJsonString(shardJson, "redirectHost", shard.redirectHost);
    ReadJsonUInt16(shardJson, "redirectTcpPort", shard.redirectTcpPort);
    ReadJsonUInt16(shardJson, "redirectUdpPort", shard.redirectUdpPort);
}

void ApplyScalePolicyJson(const std::string& policyJson, ScalePolicyConfig& policy) {
    std::string overflowPolicy;
    if (ReadJsonString(policyJson, "overflowPolicy", overflowPolicy)) {
        policy.overflowPolicy = ParseOverflowPolicy(overflowPolicy, policy.overflowPolicy);
    }

    ReadJsonUInt(policyJson, "shardSoftCap", policy.shardSoftCap);
    ReadJsonUInt(policyJson, "shardHardCap", policy.shardHardCap);
    ReadJsonUInt(policyJson, "shardSplitThreshold", policy.shardSplitThreshold);
    ReadJsonFloat(policyJson, "shardTickMsSoftLimit", policy.shardTickMsSoftLimit);
    ReadJsonFloat(policyJson, "shardTickMsHardLimit", policy.shardTickMsHardLimit);
    ReadJsonUInt(policyJson, "cellLayerDensityThreshold", policy.cellLayerDensityThreshold);
    ReadJsonFloat(policyJson, "densityCellSize", policy.densityCellSize);
    ReadJsonFloat(policyJson, "layerFullRadius", policy.layerFullRadius);
    ReadJsonFloat(policyJson, "layerMidRadius", policy.layerMidRadius);
    ReadJsonFloat(policyJson, "layerFarRadius", policy.layerFarRadius);
    ReadJsonFloat(policyJson, "layerFullRadiusCrowded", policy.layerFullRadiusCrowded);
    ReadJsonSizeT(policyJson, "observersCapNormal", policy.observersCapNormal);
    ReadJsonSizeT(policyJson, "observersCapCrowded", policy.observersCapCrowded);
    ReadJsonSizeT(policyJson, "maxPeerTransformsPerTick", policy.maxPeerTransformsPerTick);
    ReadJsonSizeT(policyJson, "maxVisibilityEventsPerTick", policy.maxVisibilityEventsPerTick);
    ReadJsonFloat(policyJson, "spawnSpacingMin", policy.spawnSpacingMin);
    ReadJsonFloat(policyJson, "spawnSpacingMax", policy.spawnSpacingMax);
    ReadJsonUInt(policyJson, "targetMaxCellDensity", policy.targetMaxCellDensity);
    ReadJsonFloat(policyJson, "snapshotIntervalNormalSeconds", policy.snapshotIntervalNormalSeconds);
    ReadJsonFloat(policyJson, "snapshotIntervalCrowdedSeconds", policy.snapshotIntervalCrowdedSeconds);
    ReadJsonFloat(policyJson, "snapshotIntervalStressedSeconds", policy.snapshotIntervalStressedSeconds);
}

} // namespace

OverflowPolicy ParseOverflowPolicy(const std::string& value, OverflowPolicy fallback) {
    const std::string normalized = StripQuotes(value);
    if (normalized == "LayerFirst") {
        return OverflowPolicy::LayerFirst;
    }
    if (normalized == "ShardFirst") {
        return OverflowPolicy::ShardFirst;
    }
    if (normalized == "LayerThenShard") {
        return OverflowPolicy::LayerThenShard;
    }
    return fallback;
}

bool ZoneScaleConfig::LoadFromJson(const std::string& json, ZoneScaleConfig& out) {
    if (json.empty()) {
        return false;
    }

    ZoneScaleConfig parsed;
    ReadJsonString(json, "zoneId", parsed.zoneId);

    const std::string shardJson = ExtractJsonObject(json, "shard");
    if (!shardJson.empty()) {
        ApplyShardJson(shardJson, parsed.shard);
    }

    const std::string policyJson = ExtractJsonObject(json, "scalePolicy");
    if (!policyJson.empty()) {
        ApplyScalePolicyJson(policyJson, parsed.scalePolicy);
    }

    out = std::move(parsed);
    return !policyJson.empty() || !shardJson.empty() || !out.zoneId.empty();
}

bool ZoneScaleConfig::TryLoadFromFile(const std::string& path, ZoneScaleConfig& out) {
    if (path.empty()) {
        return false;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return LoadFromJson(buffer.str(), out);
}

} // namespace Net
} // namespace Engine
