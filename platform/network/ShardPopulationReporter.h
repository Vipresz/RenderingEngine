#pragma once

#include "core/network/NetworkPayloads.h"
#include "core/network/ShardRegistry.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace Engine {
namespace Platform {
namespace Networking {

class NetServer;

// Reports local shard population to a gateway and mirrors overflow shard population locally.
class ShardPopulationReporter {
public:
    using SendHeartbeatFn =
        std::function<bool(const Engine::Net::PopulationHeartbeatPayload& heartbeat)>;

    void Configure(uint32_t localShardId,
                   uint32_t maxPlayers,
                   uint32_t overflowShardId,
                   SendHeartbeatFn sendHeartbeat);
    void SetOverflowPopulationProvider(std::function<uint32_t()> provider);
    void SetGatewayEndpoint(std::string host, uint16_t port);
    void Tick(NetServer& server, uint32_t localPlayerCount);

private:
    uint32_t m_localShardId = 0;
    uint32_t m_maxPlayers = 0;
    uint32_t m_overflowShardId = 0;
    SendHeartbeatFn m_sendHeartbeat;
    std::function<uint32_t()> m_overflowPopulationProvider;
    std::string m_gatewayHost;
    uint16_t m_gatewayPort = 0;
    float m_accumulatorSeconds = 0.0f;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
