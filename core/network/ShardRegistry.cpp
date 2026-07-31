#include "core/network/ShardRegistry.h"

#include <algorithm>

namespace Engine {
namespace Net {

void ShardRegistry::RegisterShard(const ShardEndpoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_endpoints[endpoint.shardId] = endpoint;
}

void ShardRegistry::UpdatePopulation(uint32_t shardId, uint32_t playerCount, uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_population[shardId] = ShardPopulation{playerCount, nowMs};
}

std::optional<ShardEndpoint> ShardRegistry::SelectShardForLogin(uint64_t nowMs, uint64_t staleAfterMs) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const ShardEndpoint* best = nullptr;
    uint32_t bestLoad = UINT32_MAX;

    for (const auto& entry : m_endpoints) {
        const ShardEndpoint& endpoint = entry.second;
        uint32_t population = 0;
        const auto popIt = m_population.find(endpoint.shardId);
        if (popIt != m_population.end()) {
            if (nowMs >= popIt->second.lastHeartbeatMs &&
                nowMs - popIt->second.lastHeartbeatMs > staleAfterMs) {
                continue;
            }
            population = popIt->second.playerCount;
        }

        const uint32_t maxPlayers = endpoint.maxPlayers > 0 ? endpoint.maxPlayers : 256;
        if (population >= maxPlayers) {
            continue;
        }

        if (population < bestLoad) {
            bestLoad = population;
            best = &endpoint;
        }
    }

    if (!best) {
        return std::nullopt;
    }
    return *best;
}

std::optional<uint32_t> ShardRegistry::GetPopulation(uint32_t shardId,
                                                   uint64_t nowMs,
                                                   uint64_t staleAfterMs) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto popIt = m_population.find(shardId);
    if (popIt == m_population.end()) {
        return std::nullopt;
    }
    if (nowMs >= popIt->second.lastHeartbeatMs &&
        nowMs - popIt->second.lastHeartbeatMs > staleAfterMs) {
        return std::nullopt;
    }
    return popIt->second.playerCount;
}

std::vector<ShardEndpoint> ShardRegistry::ListShards() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ShardEndpoint> shards;
    shards.reserve(m_endpoints.size());
    for (const auto& entry : m_endpoints) {
        shards.push_back(entry.second);
    }
    std::sort(shards.begin(), shards.end(), [](const ShardEndpoint& lhs, const ShardEndpoint& rhs) {
        return lhs.shardId < rhs.shardId;
    });
    return shards;
}

} // namespace Net
} // namespace Engine
