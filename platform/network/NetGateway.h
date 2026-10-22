#pragma once

#include "core/network/ShardRegistry.h"

#include "network/NetworkConnection.h"
#include "network/Socket.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace Engine {
namespace Platform {
namespace Networking {

// Thin login router that redirects clients to the least-loaded registered shard.
class NetGateway {
public:
    NetGateway();
    ~NetGateway();

    bool Start(const std::string& host, uint16_t port);
    void Stop();
    bool IsRunning() const { return m_running.load(std::memory_order_acquire); }
    uint16_t ListenPort() const { return m_listenSocket.LocalPort(); }

    Engine::Net::ShardRegistry& GetRegistry() { return m_registry; }
    const Engine::Net::ShardRegistry& GetRegistry() const { return m_registry; }

    void SetRequiredAuthToken(std::string token) { m_requiredAuthToken = std::move(token); }

private:
    void IoLoop();
    void HandleClient(std::unique_ptr<NetworkConnection> connection);
    void HandleMessage(NetworkConnection& connection, const Engine::Net::NetPacket& message);
    void SendLoginRedirect(NetworkConnection& connection,
                           const Engine::Net::ShardEndpoint& shard,
                           const std::string& reason);

    Socket m_listenSocket;
    std::thread m_ioThread;
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_sequence{1};
    std::string m_requiredAuthToken;
    Engine::Net::ShardRegistry m_registry;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
