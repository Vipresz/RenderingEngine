#pragma once

#include "core/network/NetPacket.h"
#include "core/network/NetworkPayloads.h"
#include "core/network/ReplicationTypes.h"
#include "core/network/CredentialStore.h"
#include "core/network/MovementCommand.h"
#include "core/network/ShardConfig.h"
#include "core/network/ScalePolicyConfig.h"
#include "core/network/ReplicationTier.h"
#include "core/network/ZoneScaleConfig.h"
#include "network/InterestManager.h"
#include "network/RateLimiter.h"
#include "network/TlsSession.h"
#include "network/MessageQueue.h"
#include "network/NetworkConnection.h"
#include "network/ServerAuthority.h"
#include "network/UdpChannel.h"

#include <glm/glm.hpp>
#include <atomic>
#include <cstdint>
#include <deque>
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

    void SetShardConfig(const Engine::Net::ShardConfig& config);
    const Engine::Net::ShardConfig& GetShardConfig() const { return m_shardConfig; }
    void SetScalePolicyConfig(const Engine::Net::ScalePolicyConfig& config);
    const Engine::Net::ScalePolicyConfig& GetScalePolicyConfig() const { return m_scalePolicy; }
    void ApplyZoneScaleConfig(const Engine::Net::ZoneScaleConfig& config);
    void ApplyLayeringState(bool layeringActive);
    void SetObservedTickP95Ms(float tickP95Ms) { m_observedTickP95Ms = tickP95Ms; }
    bool IsLayeringActive() const { return m_layeringActive; }

    bool IsRunning() const { return m_running; }
    size_t GetClientCount() const;
    size_t GetLoggedInClientCount() const;

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
    void SendTransformToInterested(const Engine::Net::TransformStatePayload& transform,
                                   Engine::Net::UserId sourceUserId,
                                   Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    bool SendAuthoritativeSelfTransform(Engine::Net::UserId sourceUserId,
                                        const Engine::Net::TransformStatePayload& transform);
    void EnqueueSelfTransform(Engine::Net::UserId sourceUserId,
                              const Engine::Net::TransformStatePayload& transform);
    size_t FlushPendingSelfTransforms(size_t maxSends);
    bool SendMovementAck(Engine::Net::UserId userId, uint64_t tick);
    void EnqueueTransformToPeers(const Engine::Net::TransformStatePayload& transform,
                                 Engine::Net::UserId sourceUserId,
                                 Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    size_t FlushPendingPeerTransforms(size_t maxSends);
    size_t GetReplicationQueueDepth() const;
    void SendInterestFilteredJoinState(Engine::Net::UserId userId, const glm::vec3& observerPosition);
    void StoreAuthoritativeTransform(Engine::Net::UserId userId,
                                     const Engine::Net::TransformStatePayload& transform);
    void ClearClientWorldState(Engine::Net::UserId userId);

    using ClientLoggedInCallback = std::function<void(Engine::Net::UserId, const std::string& displayName)>;
    using ClientDisconnectedCallback = std::function<void(Engine::Net::UserId)>;
    using GenericDataInterceptor = std::function<bool(Engine::Net::UserId, Engine::Net::GenericDataPayload&)>;
    using MovementCommandInterceptor = std::function<bool(Engine::Net::UserId, Engine::Net::MovementCommand&)>;

    void SetClientLoggedInCallback(ClientLoggedInCallback callback) {
        m_clientLoggedInCallback = std::move(callback);
    }
    void SetClientDisconnectedCallback(ClientDisconnectedCallback callback) {
        m_clientDisconnectedCallback = std::move(callback);
    }
    void SetGenericDataInterceptor(GenericDataInterceptor interceptor) {
        m_genericDataInterceptor = std::move(interceptor);
    }
    void SetMovementCommandInterceptor(MovementCommandInterceptor interceptor) {
        m_movementCommandInterceptor = std::move(interceptor);
    }
    void SetAuthoritativeMode(bool enabled) { m_authoritativeMode = enabled; }
    bool IsAuthoritativeMode() const { return m_authoritativeMode; }

    void SetMovementRateLimit(float commandsPerSecond, float burstCapacity);
    uint32_t ConsumeMovementCommandsDropped();
    void SetLoginRateLimit(float loginsPerSecond, float burstCapacity);
    uint32_t ConsumeLoginAttemptsDropped();

private:
    void IoLoop();
    size_t PollClients();
    void RemoveClient(Engine::Net::UserId userId);
    void HandleClientMessage(Engine::Net::UserId userId, const Engine::Net::NetPacket& message);
    void HandleUnreliableTransform(Engine::Net::UserId userId,
                                   const Engine::Net::TransformStatePayload& transform);
    void HandleUdpMovementCommand(Engine::Net::UserId userId, Engine::Net::MovementCommand& command);
    void HandleUdpSessionRegister(const Engine::Net::UdpSessionRegisterPayload& registration,
                                    const UdpDatagramContext& context);
    Engine::Net::UserId ResolveUdpUser(const UdpDatagramContext& context) const;
    void ClearUdpEndpoint(Engine::Net::UserId userId);
    void ClearUdpEndpointUnlocked(Engine::Net::UserId userId);
    bool IsShardAtCapacity() const;
    void FillRedirectResponse(Engine::Net::LoginResponsePayload& response) const;

    bool SendToClient(Engine::Net::UserId userId, const Engine::Net::NetPacket& message);
    bool SendUdpTransformTo(Engine::Net::UserId userId, const Engine::Net::TransformStatePayload& transform);
    bool SendUdpPayloadTo(Engine::Net::UserId userId,
                            Engine::Net::MessageType type,
                            const Engine::Net::NetBuffer& payload);
    bool DeliverTransformToObserver(Engine::Net::UserId observerId,
                                    Engine::Net::UserId sourceUserId,
                                    const Engine::Net::TransformStatePayload& transform,
                                    bool layeringActive,
                                    const Engine::Net::EffectiveLayerRadii& radii);
    void Broadcast(const Engine::Net::NetPacket& message,
                   Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    void BroadcastToInterested(const Engine::Net::NetPacket& message,
                                 Engine::Net::UserId sourceUserId,
                                 const glm::vec3& sourcePosition,
                                 Engine::Net::UserId excludeUserId = Engine::Net::kInvalidUserId);
    bool ParseEndpointKey(const std::string& endpointKey, std::string& outHost, uint16_t& outPort) const;
    uint64_t GetClientSessionToken(Engine::Net::UserId userId) const;

    static std::string MakeEndpointKey(const std::string& host, uint16_t port);

    struct PendingPeerTransform {
        Engine::Net::TransformStatePayload transform;
        Engine::Net::UserId sourceUserId = Engine::Net::kInvalidUserId;
        Engine::Net::UserId observerId = Engine::Net::kInvalidUserId;
    };

    Socket m_listenSocket;
    UdpChannel m_udpChannel;
    InterestManager m_interestManager;
    ServerAuthority m_authority;
    std::thread m_ioThread;

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
    Engine::Net::ShardConfig m_shardConfig;
    Engine::Net::ScalePolicyConfig m_scalePolicy;
    bool m_layeringActive = false;
    float m_observedTickP95Ms = 0.0f;
    TlsConfig m_tlsConfig;
    ClientLoggedInCallback m_clientLoggedInCallback;
    ClientDisconnectedCallback m_clientDisconnectedCallback;
    GenericDataInterceptor m_genericDataInterceptor;
    MovementCommandInterceptor m_movementCommandInterceptor;
    bool m_authoritativeMode = false;
    RateLimiter m_movementRateLimiter{120.0f, 30.0f};
    RateLimiter m_loginRateLimiter{100.0f, 200.0f};
    std::atomic<uint32_t> m_movementCommandsDropped{0};
    std::atomic<uint32_t> m_loginAttemptsDropped{0};

    mutable std::mutex m_replicationQueueMutex;
    std::deque<PendingPeerTransform> m_pendingPeerTransforms;
    std::deque<PendingPeerTransform> m_pendingSelfTransforms;

    std::atomic<bool> m_running{false};
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
