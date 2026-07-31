#include "network/NetGateway.h"

#include "Logger.h"
#include "core/network/MigrationToken.h"
#include "core/network/NetworkAuth.h"
#include "core/network/NetworkPayloads.h"

#include <chrono>
#include <cstring>

namespace Engine {
namespace Platform {
namespace Networking {

namespace {

uint64_t NowUnixSeconds() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
}

} // namespace

NetGateway::NetGateway() = default;

NetGateway::~NetGateway() {
    Stop();
}

bool NetGateway::Start(const std::string& host, uint16_t port) {
    if (m_running.load(std::memory_order_acquire)) {
        return true;
    }

    m_listenSocket = Socket::CreateTcp();
    if (!m_listenSocket.IsValid()) {
        return false;
    }
    m_listenSocket.SetReuseAddress(true);
    if (!m_listenSocket.Bind(host, port) || !m_listenSocket.Listen()) {
        m_listenSocket.Close();
        return false;
    }

    m_running.store(true, std::memory_order_release);
    m_ioThread = std::thread([this]() { IoLoop(); });
    LOG_INFO_FMT("NetGateway", "Gateway listening on %s:%u", host.c_str(), port);
    return true;
}

void NetGateway::Stop() {
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    m_listenSocket.Close();
    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }
}

void NetGateway::IoLoop() {
    while (m_running.load(std::memory_order_acquire)) {
        Socket clientSocket;
        if (!m_listenSocket.Accept(clientSocket)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        auto connection = std::make_unique<NetworkConnection>(std::move(clientSocket));
        std::thread([this, connection = std::move(connection)]() mutable {
            HandleClient(std::move(connection));
        }).detach();
    }
}

void NetGateway::HandleClient(std::unique_ptr<NetworkConnection> connection) {
    if (!connection) {
        return;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (m_running.load(std::memory_order_acquire) && connection->IsConnected() &&
           std::chrono::steady_clock::now() < deadline) {
        Engine::Net::NetPacket message;
        if (!connection->ReceivePacket(message)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        HandleMessage(*connection, message);
    }
    connection->Close();
}

void NetGateway::HandleMessage(NetworkConnection& connection, const Engine::Net::NetPacket& message) {
    switch (message.GetType()) {
        case Engine::Net::MessageType::LoginRequest: {
            Engine::Net::LoginRequestPayload request;
            if (!Engine::Net::LoginRequestPayload::Deserialize(message.payload, request)) {
                break;
            }

            if (!m_requiredAuthToken.empty() && !Engine::Net::VerifyAuthToken(m_requiredAuthToken, request.authToken)) {
                Engine::Net::LoginResponsePayload response;
                response.success = false;
                response.message = "unauthorized";
                connection.SendPacket(Engine::Net::MakeMessage(
                    Engine::Net::MessageType::LoginResponse, response.Serialize(), m_sequence++));
                break;
            }

            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            const auto shard = m_registry.SelectShardForLogin(nowMs);
            if (!shard) {
                Engine::Net::LoginResponsePayload response;
                response.success = false;
                response.message = "no_shard_available";
                connection.SendPacket(Engine::Net::MakeMessage(
                    Engine::Net::MessageType::LoginResponse, response.Serialize(), m_sequence++));
                break;
            }

            SendLoginRedirect(connection, *shard, "gateway_redirect");
            break;
        }
        case Engine::Net::MessageType::PopulationHeartbeat: {
            Engine::Net::PopulationHeartbeatPayload heartbeat;
            if (!Engine::Net::PopulationHeartbeatPayload::Deserialize(message.payload, heartbeat)) {
                break;
            }
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            m_registry.UpdatePopulation(heartbeat.shardId, heartbeat.playerCount, nowMs);
            break;
        }
        case Engine::Net::MessageType::PopulationQuery: {
            Engine::Net::PopulationQueryPayload query;
            if (!Engine::Net::PopulationQueryPayload::Deserialize(message.payload, query)) {
                break;
            }
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            const auto population = m_registry.GetPopulation(query.shardId, nowMs);
            Engine::Net::PopulationHeartbeatPayload response;
            response.shardId = query.shardId;
            response.playerCount = population.value_or(0);
            const auto shards = m_registry.ListShards();
            for (const Engine::Net::ShardEndpoint& shard : shards) {
                if (shard.shardId == query.shardId) {
                    response.maxPlayers = shard.maxPlayers;
                    break;
                }
            }
            connection.SendPacket(Engine::Net::MakeMessage(
                Engine::Net::MessageType::PopulationHeartbeat, response.Serialize(), m_sequence++));
            break;
        }
        default:
            break;
    }
}

void NetGateway::SendLoginRedirect(NetworkConnection& connection,
                                   const Engine::Net::ShardEndpoint& shard,
                                   const std::string& reason) {
    Engine::Net::LoginResponsePayload response;
    response.success = false;
    response.redirect = true;
    response.shardId = shard.shardId;
    response.redirectHost = shard.host;
    response.redirectTcpPort = shard.tcpPort;
    response.redirectUdpPort = shard.udpPort != 0 ? shard.udpPort
                                                  : static_cast<uint16_t>(shard.tcpPort + 1);
    response.message = reason;
    connection.SendPacket(
        Engine::Net::MakeMessage(Engine::Net::MessageType::LoginResponse, response.Serialize(), m_sequence++));
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
