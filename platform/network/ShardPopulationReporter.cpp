#include "network/ShardPopulationReporter.h"

#include "network/NetServer.h"
#include "network/NetworkConnection.h"
#include "network/Socket.h"

#include "core/network/NetworkPayloads.h"

#include <chrono>
#include <thread>

namespace Engine {
namespace Platform {
namespace Networking {

namespace {

bool SendGatewayMessage(const std::string& host,
                        uint16_t port,
                        const Engine::Net::NetPacket& packet,
                        Engine::Net::NetPacket* response = nullptr) {
    Socket socket = Socket::CreateTcp();
    if (!socket.IsValid()) {
        return false;
    }
    if (!socket.Connect(host, port)) {
        socket.Close();
        return false;
    }

    NetworkConnection connection(std::move(socket));
    if (!connection.SendPacket(packet)) {
        connection.Close();
        return false;
    }

    if (!response) {
        connection.Close();
        return true;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (connection.ReceivePacket(*response)) {
            connection.Close();
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    connection.Close();
    return false;
}

} // namespace

void ShardPopulationReporter::Configure(uint32_t localShardId,
                                        uint32_t maxPlayers,
                                        uint32_t overflowShardId,
                                        SendHeartbeatFn sendHeartbeat) {
    m_localShardId = localShardId;
    m_maxPlayers = maxPlayers;
    m_overflowShardId = overflowShardId;
    m_sendHeartbeat = std::move(sendHeartbeat);
}

void ShardPopulationReporter::SetOverflowPopulationProvider(std::function<uint32_t()> provider) {
    m_overflowPopulationProvider = std::move(provider);
}

void ShardPopulationReporter::SetGatewayEndpoint(std::string host, uint16_t port) {
    m_gatewayHost = std::move(host);
    m_gatewayPort = port;
}

void ShardPopulationReporter::Tick(NetServer& server, uint32_t localPlayerCount) {
    constexpr float kHeartbeatIntervalSeconds = 2.0f;
    m_accumulatorSeconds += 1.0f / 60.0f;
    if (m_accumulatorSeconds < kHeartbeatIntervalSeconds) {
        return;
    }
    m_accumulatorSeconds = 0.0f;

    if (m_sendHeartbeat) {
        Engine::Net::PopulationHeartbeatPayload heartbeat;
        heartbeat.shardId = m_localShardId;
        heartbeat.playerCount = localPlayerCount;
        heartbeat.maxPlayers = m_maxPlayers;
        m_sendHeartbeat(heartbeat);
    } else if (!m_gatewayHost.empty() && m_gatewayPort != 0) {
        Engine::Net::PopulationHeartbeatPayload heartbeat;
        heartbeat.shardId = m_localShardId;
        heartbeat.playerCount = localPlayerCount;
        heartbeat.maxPlayers = m_maxPlayers;
        SendGatewayMessage(
            m_gatewayHost,
            m_gatewayPort,
            Engine::Net::MakeMessage(Engine::Net::MessageType::PopulationHeartbeat, heartbeat.Serialize()));
    }

    uint32_t overflowPopulation = 0;
    if (m_overflowPopulationProvider) {
        overflowPopulation = m_overflowPopulationProvider();
    } else if (m_overflowShardId != 0 && !m_gatewayHost.empty() && m_gatewayPort != 0) {
        Engine::Net::PopulationQueryPayload query;
        query.shardId = m_overflowShardId;
        Engine::Net::NetPacket response;
        if (SendGatewayMessage(m_gatewayHost,
                               m_gatewayPort,
                               Engine::Net::MakeMessage(Engine::Net::MessageType::PopulationQuery,
                                                        query.Serialize()),
                               &response) &&
            response.GetType() == Engine::Net::MessageType::PopulationHeartbeat) {
            Engine::Net::PopulationHeartbeatPayload heartbeat;
            if (Engine::Net::PopulationHeartbeatPayload::Deserialize(response.payload, heartbeat)) {
                overflowPopulation = heartbeat.playerCount;
            }
        }
    }

    if (m_overflowShardId != 0) {
        server.SetOverflowLayerPopulation(overflowPopulation);
    }
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
