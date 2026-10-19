#pragma once

#include "core/network/ShardConfig.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Net {

struct ShardEndpoint {
    uint32_t shardId = 0;
    std::string host;
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;
    uint32_t maxPlayers = 256;
};

struct ShardPopulation {
    uint32_t playerCount = 0;
    uint64_t lastHeartbeatMs = 0;
};

// In-memory shard directory used by the login gateway and population sync.
class ShardRegistry {
public:
    void RegisterShard(const ShardEndpoint& endpoint);
    void UpdatePopulation(uint32_t shardId, uint32_t playerCount, uint64_t nowMs);
    std::optional<ShardEndpoint> SelectShardForLogin(uint64_t nowMs, uint64_t staleAfterMs = 15000) const;
    std::optional<uint32_t> GetPopulation(uint32_t shardId, uint64_t nowMs, uint64_t staleAfterMs = 15000) const;
    std::vector<ShardEndpoint> ListShards() const;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<uint32_t, ShardEndpoint> m_endpoints;
    std::unordered_map<uint32_t, ShardPopulation> m_population;
};

} // namespace Net
} // namespace Engine
