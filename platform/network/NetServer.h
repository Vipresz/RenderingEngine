#pragma once

#include "core/network/NetPacket.h"
#include "core/network/NetworkPayloads.h"
#include "network/MessageQueue.h"
#include "network/NetworkConnection.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Platform {
namespace Networking {

struct ConnectedClient {
    Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
    std::string displayName;
    std::unique_ptr<NetworkConnection> connection;
    std::thread receiveThread;
};

class NetServer {
public:
    NetServer();
    ~NetServer();

    bool Start(const std::string& host, uint16_t port);
    void Stop();

    bool IsRunning() const { return m_running; }
    size_t GetClientCount() const;

    void BroadcastGenericData(const Engine::Net::GenericDataPayload& payload,
                              Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);

private:
    void AcceptLoop();
    void ClientReceiveLoop(Engine::Net::UserId userId);
    void RemoveClient(Engine::Net::UserId userId);
    void HandleClientMessage(Engine::Net::UserId userId, const Engine::Net::NetPacket& message);

    bool SendToClient(Engine::Net::UserId userId, const Engine::Net::NetPacket& message);
    void Broadcast(const Engine::Net::NetPacket& message,
                   Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    void SendExistingPeersTo(Engine::Net::UserId newUserId);

    Socket m_listenSocket;
    std::thread m_acceptThread;

    mutable std::mutex m_clientsMutex;
    std::unordered_map<Engine::Net::UserId, std::unique_ptr<ConnectedClient>> m_clients;
    Engine::Net::UserId m_nextUserId = 1;
    uint64_t m_sequence = 1;

    std::atomic<bool> m_running{false};
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
