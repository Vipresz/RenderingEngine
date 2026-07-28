#pragma once

#include "core/network/NetPacket.h"
#include "core/network/NetworkPayloads.h"
#include "core/network/ReplicationTypes.h"
#include "core/network/CredentialStore.h"
#include "network/InterestManager.h"
#include "network/TlsSession.h"
#include "network/MessageQueue.h"
#include "network/NetworkConnection.h"
#include "network/ServerAuthority.h"
#include "network/UdpChannel.h"

#include <glm/glm.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
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
    uint64_t udpSessionToken = 0;
    std::unique_ptr<NetworkConnection> connection;
    std::thread receiveThread;
};

class NetServer {
public:
    NetServer();
    ~NetServer();

    bool Start(const std::string& host, uint16_t port, uint16_t udpPort = Engine::Net::kDefaultUdpPort);
    void Stop();

    bool IsUdpBound() const { return m_udpChannel.IsOpen(); }
    bool HasUdpEndpointFor(Engine::Net::UserId userId) const;

    void SetRequiredAuthToken(std::string token) { m_requiredAuthToken = std::move(token); }
    const std::string& GetRequiredAuthToken() const { return m_requiredAuthToken; }

    Engine::Net::CredentialStore& GetCredentialStore() { return m_credentialStore; }
    const Engine::Net::CredentialStore& GetCredentialStore() const { return m_credentialStore; }
    void SetTlsConfig(const TlsConfig& config) { m_tlsConfig = config; }

    bool IsRunning() const { return m_running; }
    size_t GetClientCount() const;

    InterestManager& GetInterestManager() { return m_interestManager; }
    const InterestManager& GetInterestManager() const { return m_interestManager; }
    ServerAuthority& GetServerAuthority() { return m_authority; }
    const ServerAuthority& GetServerAuthority() const { return m_authority; }

    void BroadcastGenericData(const Engine::Net::GenericDataPayload& payload,
                              Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    void BroadcastWorldSnapshot(const Engine::Net::WorldSnapshotPayload& snapshot,
                                Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    void SendWorldSnapshotTo(Engine::Net::UserId userId, const Engine::Net::WorldSnapshotPayload& snapshot);
    void BroadcastPacket(const Engine::Net::NetPacket& message,
                         Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    void BroadcastPacketToInterested(const Engine::Net::NetPacket& message,
                                     Engine::Net::UserId sourceUserId,
                                     const glm::vec3& sourcePosition,
                                     Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    bool SendPacketToClient(Engine::Net::UserId userId, const Engine::Net::NetPacket& message);
    void StoreAuthoritativeTransform(Engine::Net::UserId userId,
                                     const Engine::Net::TransformStatePayload& transform);
    void ClearClientWorldState(Engine::Net::UserId userId);

    using ClientLoggedInCallback = std::function<void(Engine::Net::UserId, const std::string& displayName)>;
    using ClientDisconnectedCallback = std::function<void(Engine::Net::UserId)>;
    using GenericDataInterceptor = std::function<bool(Engine::Net::UserId, Engine::Net::GenericDataPayload&)>;

    void SetClientLoggedInCallback(ClientLoggedInCallback callback) {
        m_clientLoggedInCallback = std::move(callback);
    }
    void SetClientDisconnectedCallback(ClientDisconnectedCallback callback) {
        m_clientDisconnectedCallback = std::move(callback);
    }
    void SetGenericDataInterceptor(GenericDataInterceptor interceptor) {
        m_genericDataInterceptor = std::move(interceptor);
    }
    void SetAuthoritativeMode(bool enabled) { m_authoritativeMode = enabled; }
    bool IsAuthoritativeMode() const { return m_authoritativeMode; }

private:
    void AcceptLoop();
    void ClientReceiveLoop(Engine::Net::UserId userId);
    void RemoveClient(Engine::Net::UserId userId);
    void HandleClientMessage(Engine::Net::UserId userId, const Engine::Net::NetPacket& message);
    void HandleUnreliableTransform(Engine::Net::UserId userId,
                                   const Engine::Net::TransformStatePayload& transform);
    void HandleUdpSessionRegister(const Engine::Net::UdpSessionRegisterPayload& registration,
                                    const UdpDatagramContext& context);
    Engine::Net::UserId ResolveUdpUser(const UdpDatagramContext& context) const;
    void ClearUdpEndpoint(Engine::Net::UserId userId);

    bool SendToClient(Engine::Net::UserId userId, const Engine::Net::NetPacket& message);
    void Broadcast(const Engine::Net::NetPacket& message,
                   Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    void BroadcastToInterested(const Engine::Net::NetPacket& message,
                                 Engine::Net::UserId sourceUserId,
                                 const glm::vec3& sourcePosition,
                                 Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    void SendExistingPeersTo(Engine::Net::UserId newUserId);
    void SendExistingPeerTransformsTo(Engine::Net::UserId newUserId);

    static std::string MakeEndpointKey(const std::string& host, uint16_t port);

    Socket m_listenSocket;
    UdpChannel m_udpChannel;
    InterestManager m_interestManager;
    ServerAuthority m_authority;
    std::thread m_acceptThread;

    mutable std::mutex m_clientsMutex;
    mutable std::mutex m_worldMutex;
    std::unordered_map<Engine::Net::UserId, std::unique_ptr<ConnectedClient>> m_clients;
    std::unordered_map<Engine::Net::UserId, Engine::Net::TransformStatePayload> m_lastTransforms;
    std::unordered_map<std::string, Engine::Net::UserId> m_endpointToUser;
    std::unordered_map<Engine::Net::UserId, std::string> m_userToEndpoint;
    Engine::Net::UserId m_nextUserId = 1;
    std::atomic<uint64_t> m_sequence{1};
    std::string m_requiredAuthToken;
    Engine::Net::CredentialStore m_credentialStore;
    TlsConfig m_tlsConfig;
    ClientLoggedInCallback m_clientLoggedInCallback;
    ClientDisconnectedCallback m_clientDisconnectedCallback;
    GenericDataInterceptor m_genericDataInterceptor;
    bool m_authoritativeMode = false;

    std::atomic<bool> m_running{false};
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
