#include "network/NetServer.h"



#include "Logger.h"

#include "core/network/NetworkAuth.h"
#include "core/network/MigrationToken.h"
#include "core/network/ReplicationTier.h"
#include "core/network/ReplicationTierThrottle.h"
#include "core/network/LayerInstancePolicy.h"
#include "core/network/ServerSecurityPolicy.h"

#include <chrono>

#include <random>

#include <thread>

#include <algorithm>

#include <unordered_map>

#include <cctype>



#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN

#define WIN32_LEAN_AND_MEAN

#endif

#include <winsock2.h>

#endif



using EngineLogging::Logger;



namespace {



uint64_t GenerateSessionToken() {

    std::random_device rd;

    const uint64_t a = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());

    const uint64_t b = (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());

    return a ^ b;

}



std::string SanitizeDisplayName(const std::string& raw) {

    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };

    auto begin = std::find_if(raw.begin(), raw.end(), notSpace);

    if (begin == raw.end()) {

        return {};

    }

    auto end = std::find_if(std::make_reverse_iterator(raw.end()),

                            std::make_reverse_iterator(begin),

                            notSpace)

                   .base();



    std::string trimmed(begin, end);

    trimmed.erase(std::remove_if(trimmed.begin(),

                                 trimmed.end(),

                                 [](unsigned char c) { return c < 32 || c == 127; }),

                  trimmed.end());



    constexpr size_t kMaxDisplayNameLength = 32;

    if (trimmed.size() > kMaxDisplayNameLength) {

        trimmed.resize(kMaxDisplayNameLength);

    }

    return trimmed;

}



bool AuthTokenMatches(const std::string& required, const std::string& provided) {
    return Engine::Net::VerifyAuthToken(required, provided);
}



} // namespace



namespace Engine {

namespace Platform {

namespace Networking {



NetServer::NetServer() : m_cellLayerAssigner(m_scalePolicy) {}



NetServer::~NetServer() {

    Stop();

}



bool NetServer::Start(const std::string& host, uint16_t port, uint16_t udpPort) {

    Stop();

    if (!Engine::Net::CanAcceptLogin(m_securityPolicy, m_requiredAuthToken, m_credentialStore)) {
        LOG_ERROR("NetServer",
                  "Refusing to start: enforceAuthentication is enabled but no auth token or "
                  "credential store is configured");
        return false;
    }

    if (!Socket::InitializeNetworking()) {

        return false;

    }



    m_listenSocket = Socket::CreateTcp();

    if (!m_listenSocket.IsValid() ||

        !m_listenSocket.SetReuseAddress(true) ||

        !m_listenSocket.SetNonBlocking(true) ||

        !m_listenSocket.Bind(host, port) ||

        !m_listenSocket.Listen(1024)) {

        LOG_ERROR_FMT("NetServer", "Failed to bind/listen on %s:%u", host.c_str(), port);

        m_listenSocket.Close();

        return false;

    }



    if (!m_udpChannel.Bind(host, udpPort)) {

        LOG_ERROR_FMT("NetServer", "UDP bind failed on %s:%u", host.c_str(), udpPort);

        m_listenSocket.Close();

        return false;

    }



    m_udpChannel.SetUnreliableTransformHandler(

        [this](const Engine::Net::TransformStatePayload& transform, const UdpDatagramContext& context) {

            if (m_authoritativeMode) {

                return;

            }

            const Engine::Net::UserId userId = ResolveUdpUser(context);

            if (userId == Engine::Net::kInvalidUserId) {

                return;

            }

            Engine::Net::TransformStatePayload authorized = transform;

            authorized.userId = userId;

            HandleUnreliableTransform(userId, authorized);

        });

    m_udpChannel.SetUdpMovementCommandHandler(

        [this](const Engine::Net::MovementCommand& command, const UdpDatagramContext& context) {

            if (!m_authoritativeMode) {

                return;

            }

            const Engine::Net::UserId userId = ResolveUdpUser(context);

            if (userId == Engine::Net::kInvalidUserId) {

                return;

            }

            Engine::Net::MovementCommand mutableCommand = command;

            HandleUdpMovementCommand(userId, mutableCommand);

        });

    m_udpChannel.SetUdpSessionRegisterHandler(

        [this](const Engine::Net::UdpSessionRegisterPayload& registration, const UdpDatagramContext& context) {

            HandleUdpSessionRegister(registration, context);

        });

    m_udpChannel.SetSessionTokenResolver([this](const UdpDatagramContext& context) -> uint64_t {

        const Engine::Net::UserId userId = ResolveUdpUser(context);

        if (userId == Engine::Net::kInvalidUserId) {

            return 0;

        }

        return GetClientSessionToken(userId);

    });

    m_udpChannel.SetEncryptPayloads(m_securityPolicy.encryptUdpPayloads);
    m_udpChannel.SetAuthTokenForEncryption(m_requiredAuthToken);



    m_running = true;

    m_ioThread = std::thread([this]() { IoLoop(); });

    LOG_INFO_FMT("NetServer", "Listening TCP %s:%u UDP %s:%u", host.c_str(), port, host.c_str(), udpPort);

    return true;

}



void NetServer::Stop() {

    m_running = false;

    {
        std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
        m_pendingPeerTransforms.clear();
    }

    m_listenSocket.Close();

    m_udpChannel.Close();

    {

        std::lock_guard<std::mutex> lock(m_worldMutex);

        m_lastTransforms.clear();

        m_endpointToUser.clear();

        m_userToEndpoint.clear();

        m_interestManager.Clear();

        m_authority = ServerAuthority();

    }



    if (m_ioThread.joinable()) {

        m_ioThread.join();

    }



    {

        std::lock_guard<std::mutex> lock(m_clientsMutex);

        for (auto& entry : m_clients) {

            if (entry.second && entry.second->connection) {

                entry.second->connection->Close();

            }

        }

        m_clients.clear();

    }

}



size_t NetServer::GetClientCount() const {

    std::lock_guard<std::mutex> lock(m_clientsMutex);

    return m_clients.size();

}



size_t NetServer::GetLoggedInClientCount() const {

    std::lock_guard<std::mutex> lock(m_clientsMutex);

    return CountLoggedInClientsUnlocked();

}

size_t NetServer::CountLoggedInClientsUnlocked() const {
    size_t count = 0;
    for (const auto& entry : m_clients) {
        if (entry.second && entry.second->loggedIn) {
            ++count;
        }
    }
    return count;
}

float NetServer::DecodeTickP95Ms(uint32_t bits) {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t NetServer::EncodeTickP95Ms(float tickP95Ms) {
    uint32_t bits = 0;
    std::memcpy(&bits, &tickP95Ms, sizeof(bits));
    return bits;
}

void NetServer::SetObservedTickP95Ms(float tickP95Ms) {
    m_observedTickP95Bits.store(EncodeTickP95Ms(tickP95Ms), std::memory_order_relaxed);
}

float NetServer::GetObservedTickP95Ms() const {
    return DecodeTickP95Ms(m_observedTickP95Bits.load(std::memory_order_relaxed));
}

size_t NetServer::ReplicationQueueMaxDepth() const {
    const size_t configured = m_scalePolicy.maxPeerTransformsPerTick;
    return configured > 0 ? configured : 8192;
}

void NetServer::RequeueFailedTransforms(std::deque<PendingPeerTransform>& failed) {
    if (failed.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
    const size_t maxDepth = ReplicationQueueMaxDepth();
    for (auto it = failed.rbegin(); it != failed.rend(); ++it) {
        if (m_pendingPeerTransforms.size() >= maxDepth) {
            break;
        }
        m_pendingPeerTransforms.push_front(*it);
    }
}



bool NetServer::HasUdpEndpointFor(Engine::Net::UserId userId) const {

    std::lock_guard<std::mutex> lock(m_worldMutex);

    return m_userToEndpoint.find(userId) != m_userToEndpoint.end();

}



void NetServer::BroadcastGenericData(const Engine::Net::GenericDataPayload& payload,

                                     Engine::Net::UserId excludeUserId) {

    Broadcast(Engine::Net::MakeMessage(

                  Engine::Net::MessageType::GenericData, payload.Serialize(), m_sequence++),

              excludeUserId);

}



void NetServer::BroadcastWorldSnapshot(const Engine::Net::WorldSnapshotPayload& snapshot,

                                       Engine::Net::UserId excludeUserId) {

    Broadcast(Engine::Net::MakeMessage(

                  Engine::Net::MessageType::WorldSnapshot, snapshot.Serialize(), m_sequence++),

              excludeUserId);

}



void NetServer::SendWorldSnapshotTo(Engine::Net::UserId userId, const Engine::Net::WorldSnapshotPayload& snapshot) {

    SendToClient(userId, Engine::Net::MakeMessage(

                             Engine::Net::MessageType::WorldSnapshot, snapshot.Serialize(), m_sequence++));

}



void NetServer::BroadcastPacket(const Engine::Net::NetPacket& message, Engine::Net::UserId excludeUserId) {

    Broadcast(message, excludeUserId);

}



void NetServer::BroadcastPacketToInterested(const Engine::Net::NetPacket& message,

                                            Engine::Net::UserId sourceUserId,

                                            const glm::vec3& sourcePosition,

                                            Engine::Net::UserId excludeUserId) {

    BroadcastToInterested(message, sourceUserId, sourcePosition, excludeUserId);

}



bool NetServer::SendPacketToClient(Engine::Net::UserId userId, const Engine::Net::NetPacket& message) {

    return SendToClient(userId, message);

}



bool NetServer::DeliverTransformToObserver(Engine::Net::UserId observerId,
                                           Engine::Net::UserId sourceUserId,
                                           const Engine::Net::TransformStatePayload& transform,
                                           bool layeringActive,
                                           const Engine::Net::EffectiveLayerRadii& radii) {
    Engine::Net::TransformStatePayload outgoing = transform;
    Engine::Net::ReplicationTier tier = Engine::Net::ReplicationTier::Full;
    if (layeringActive && observerId != sourceUserId) {
        glm::vec3 observerPosition = transform.position;
        {
            std::lock_guard<std::mutex> lock(m_worldMutex);
            if (!m_interestManager.TryGetEntityPosition(observerId, observerPosition)) {
                observerPosition = transform.position;
            }
        }
        const glm::vec3 delta = transform.position - observerPosition;
        tier = Engine::Net::ClassifyReplicationTier(glm::dot(delta, delta), radii);
        if (m_cellLayerAssigner.ShouldIsolateLayers(observerId, sourceUserId)) {
            tier = Engine::Net::MaxReplicationTier(tier, Engine::Net::ReplicationTier::Mid);
        }
        if (tier == Engine::Net::ReplicationTier::Hidden) {
            return false;
        }
        if (!Engine::Net::ShouldSendTransformForTier(tier, GetReplicationTickNumber())) {
            return true;
        }
        outgoing = Engine::Net::ReduceTransformForTier(transform, tier);
    }

    if (!SendUdpTransformTo(observerId, outgoing)) {
        return SendToClient(observerId, Engine::Net::MakeMessage(
                                         Engine::Net::MessageType::TransformState,
                                         outgoing.Serialize(),
                                         m_sequence++));
    }
    return true;
}

size_t NetServer::GetReplicationQueueDepth() const {
    std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
    return m_pendingPeerTransforms.size();
}

size_t NetServer::FlushPendingPeerTransforms(size_t maxSends) {
    if (maxSends == 0) {
        return 0;
    }

    std::vector<PendingPeerTransform> batch;
    batch.reserve(maxSends);
    {
        std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
        while (!m_pendingPeerTransforms.empty() && batch.size() < maxSends) {
            batch.push_back(std::move(m_pendingPeerTransforms.front()));
            m_pendingPeerTransforms.pop_front();
        }
    }

    if (batch.empty()) {
        return 0;
    }

    bool layeringActive = false;
    Engine::Net::EffectiveLayerRadii radii;
    const Engine::Net::ScalePolicyConfig policySnapshot = m_scalePolicy;
    uint32_t connectedPlayers = 0;
    uint32_t cellDensity = 0;
    {
        std::lock_guard<std::mutex> clientsLock(m_clientsMutex);
        connectedPlayers = static_cast<uint32_t>(CountLoggedInClientsUnlocked());
    }
    {
        std::lock_guard<std::mutex> worldLock(m_worldMutex);
        cellDensity = m_interestManager.GetMaxCellDensity();
        layeringActive = policySnapshot.IsLayeringActive(connectedPlayers, cellDensity);
        radii = Engine::Net::ComputeEffectiveRadii(policySnapshot, layeringActive);
    }

    size_t sent = 0;
    std::deque<PendingPeerTransform> failed;
    std::unordered_map<Engine::Net::UserId, std::vector<Engine::Net::TransformStatePayload>> batchedByObserver;
    for (const PendingPeerTransform& pending : batch) {
        Engine::Net::TransformStatePayload outgoing = pending.transform;
        Engine::Net::ReplicationTier tier = Engine::Net::ReplicationTier::Full;
        if (layeringActive && pending.observerId != pending.sourceUserId) {
            glm::vec3 observerPosition = pending.transform.position;
            {
                std::lock_guard<std::mutex> lock(m_worldMutex);
                if (!m_interestManager.TryGetEntityPosition(pending.observerId, observerPosition)) {
                    observerPosition = pending.transform.position;
                }
            }
            const glm::vec3 delta = pending.transform.position - observerPosition;
            tier = Engine::Net::ClassifyReplicationTier(glm::dot(delta, delta), radii);
            if (m_cellLayerAssigner.ShouldIsolateLayers(pending.observerId, pending.sourceUserId)) {
                tier = Engine::Net::MaxReplicationTier(tier, Engine::Net::ReplicationTier::Mid);
            }
            if (tier == Engine::Net::ReplicationTier::Hidden) {
                continue;
            }
            if (!Engine::Net::ShouldSendTransformForTier(tier, GetReplicationTickNumber())) {
                ++sent;
                continue;
            }
            outgoing = Engine::Net::ReduceTransformForTier(pending.transform, tier);
        }

        batchedByObserver[pending.observerId].push_back(std::move(outgoing));
    }

    for (auto& observerEntry : batchedByObserver) {
        std::vector<Engine::Net::TransformStatePayload>& transforms = observerEntry.second;
        size_t index = 0;
        while (index < transforms.size()) {
            const size_t batchCount = (std::min)(transforms.size() - index,
                                                 static_cast<size_t>(
                                                     Engine::Net::TransformBatchPayload::kMaxTransformsPerBatch));
            const std::vector<Engine::Net::TransformStatePayload> slice(
                transforms.begin() + static_cast<std::ptrdiff_t>(index),
                transforms.begin() + static_cast<std::ptrdiff_t>(index + batchCount));
            if (SendUdpTransformBatchTo(observerEntry.first, slice)) {
                sent += batchCount;
            } else {
                for (size_t i = 0; i < batchCount; ++i) {
                    PendingPeerTransform failedPending;
                    failedPending.observerId = observerEntry.first;
                    failedPending.sourceUserId = slice[i].userId;
                    failedPending.transform = slice[i];
                    failed.push_back(std::move(failedPending));
                }
            }
            index += batchCount;
        }
    }
    RequeueFailedTransforms(failed);
    return sent;
}

void NetServer::SendTransformToInterested(const Engine::Net::TransformStatePayload& transform,

                                            Engine::Net::UserId sourceUserId,

                                            Engine::Net::UserId excludeUserId) {
    if (sourceUserId != excludeUserId) {
        SendAuthoritativeSelfTransform(sourceUserId, transform);
    }
    EnqueueTransformToPeers(transform, sourceUserId, excludeUserId);
}

bool NetServer::SendAuthoritativeSelfTransform(Engine::Net::UserId sourceUserId,
                                               const Engine::Net::TransformStatePayload& transform) {
    Engine::Net::EffectiveLayerRadii radii;
    return DeliverTransformToObserver(sourceUserId, sourceUserId, transform, false, radii);
}

void NetServer::EnqueueSelfTransform(Engine::Net::UserId sourceUserId,
                                     const Engine::Net::TransformStatePayload& transform) {
    PendingPeerTransform pending;
    pending.transform = transform;
    pending.sourceUserId = sourceUserId;
    pending.observerId = sourceUserId;
    std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
    const size_t maxDepth = ReplicationQueueMaxDepth();
    while (m_pendingSelfTransforms.size() >= maxDepth && !m_pendingSelfTransforms.empty()) {
        m_pendingSelfTransforms.pop_front();
    }
    m_pendingSelfTransforms.push_back(std::move(pending));
}

size_t NetServer::FlushPendingSelfTransforms(size_t maxSends) {
    if (maxSends == 0) {
        return 0;
    }

    std::vector<PendingPeerTransform> batch;
    batch.reserve(maxSends);
    {
        std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
        while (!m_pendingSelfTransforms.empty() && batch.size() < maxSends) {
            batch.push_back(std::move(m_pendingSelfTransforms.front()));
            m_pendingSelfTransforms.pop_front();
        }
    }

    Engine::Net::EffectiveLayerRadii radii;
    size_t sent = 0;
    std::deque<PendingPeerTransform> failed;
    for (const PendingPeerTransform& pending : batch) {
        if (DeliverTransformToObserver(pending.observerId,
                                       pending.sourceUserId,
                                       pending.transform,
                                       false,
                                       radii)) {
            ++sent;
        } else {
            failed.push_back(pending);
        }
    }

    if (!failed.empty()) {
        std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
        const size_t maxDepth = ReplicationQueueMaxDepth();
        for (auto it = failed.rbegin(); it != failed.rend(); ++it) {
            if (m_pendingSelfTransforms.size() >= maxDepth) {
                break;
            }
            m_pendingSelfTransforms.push_front(*it);
        }
    }
    return sent;
}

bool NetServer::SendMovementAck(Engine::Net::UserId userId, uint64_t tick) {
    Engine::Net::MovementAckPayload ack;
    ack.userId = userId;
    ack.tick = tick;
    const Engine::Net::NetBuffer payload = ack.Serialize();

    if (SendUdpPayloadTo(userId, Engine::Net::MessageType::MovementAck, payload)) {
        return true;
    }

    return SendToClient(userId, Engine::Net::MakeMessage(
                                   Engine::Net::MessageType::MovementAck, payload, m_sequence++));
}

void NetServer::EnqueueTransformToPeers(const Engine::Net::TransformStatePayload& transform,
                                        Engine::Net::UserId sourceUserId,
                                        Engine::Net::UserId excludeUserId) {
    std::vector<Engine::Net::UserId> observers;
    {
        std::lock_guard<std::mutex> lock(m_worldMutex);
        observers = m_interestManager.GetInterestedObservers(sourceUserId, transform.position);
    }

    std::lock_guard<std::mutex> lock(m_replicationQueueMutex);
    const size_t maxDepth = ReplicationQueueMaxDepth();
    for (Engine::Net::UserId observerId : observers) {
        if (observerId == excludeUserId) {
            continue;
        }
        while (m_pendingPeerTransforms.size() >= maxDepth && !m_pendingPeerTransforms.empty()) {
            m_pendingPeerTransforms.pop_front();
        }
        PendingPeerTransform pending;
        pending.transform = transform;
        pending.sourceUserId = sourceUserId;
        pending.observerId = observerId;
        m_pendingPeerTransforms.push_back(std::move(pending));
    }
}



void NetServer::SendInterestFilteredJoinState(Engine::Net::UserId userId, const glm::vec3& observerPosition) {

    std::vector<Engine::Net::UserId> nearbyUsers;

    {

        std::lock_guard<std::mutex> lock(m_worldMutex);

        nearbyUsers = m_interestManager.QueryNearbyUsers(observerPosition, m_interestManager.GetDefaultInterestRadius());

    }



    for (Engine::Net::UserId peerId : nearbyUsers) {

        if (peerId == userId) {

            continue;

        }



        std::string displayName;

        {

            std::lock_guard<std::mutex> lock(m_clientsMutex);

            const auto it = m_clients.find(peerId);

            if (it == m_clients.end() || !it->second || it->second->displayName.empty()) {

                continue;

            }

            if (!it->second->loggedIn) {

                continue;

            }

            displayName = it->second->displayName;

        }



        Engine::Net::PeerEventPayload joined;

        joined.userId = peerId;

        joined.displayName = displayName;

        SendToClient(userId, Engine::Net::MakeMessage(

                                 Engine::Net::MessageType::PeerJoined, joined.Serialize(), m_sequence++));



        Engine::Net::TransformStatePayload transform;

        {

            std::lock_guard<std::mutex> lock(m_worldMutex);

            const auto transformIt = m_lastTransforms.find(peerId);

            if (transformIt == m_lastTransforms.end()) {

                continue;

            }

            transform = transformIt->second;

        }



        if (!SendUdpTransformTo(userId, transform)) {

            SendToClient(userId, Engine::Net::MakeMessage(

                                     Engine::Net::MessageType::TransformState,

                                     transform.Serialize(),

                                     m_sequence++));

        }

    }

}



void NetServer::StoreAuthoritativeTransform(Engine::Net::UserId userId,

                                            const Engine::Net::TransformStatePayload& transform) {

    std::lock_guard<std::mutex> lock(m_worldMutex);

    m_lastTransforms[userId] = transform;

    m_authority.ApplyAuthoritative(userId, transform);

    m_interestManager.UpdateEntity(userId, transform.position);
    UpdateCellLayerForPlayer(userId, transform.position);

}



void NetServer::ClearClientWorldState(Engine::Net::UserId userId) {

    std::lock_guard<std::mutex> lock(m_worldMutex);

    m_lastTransforms.erase(userId);

    m_authority.RemoveUser(userId);

    m_interestManager.RemoveEntity(userId);
    m_cellLayerAssigner.RemovePlayer(userId);

}



void NetServer::SetMovementRateLimit(float commandsPerSecond, float burstCapacity) {
    m_movementRateLimiter.SetRate(commandsPerSecond, burstCapacity);
}

uint32_t NetServer::ConsumeMovementCommandsDropped() {
    return m_movementCommandsDropped.exchange(0, std::memory_order_relaxed);
}

void NetServer::SetLoginRateLimit(float loginsPerSecond, float burstCapacity) {
    m_loginRateLimiter.SetRate(loginsPerSecond, burstCapacity);
}

uint32_t NetServer::ConsumeLoginAttemptsDropped() {
    return m_loginAttemptsDropped.exchange(0, std::memory_order_relaxed);
}

void NetServer::IoLoop() {
    while (m_running) {
        bool didWork = false;
        didWork = m_udpChannel.PollAndDrain() > 0;

        Socket clientSocket;
        while (m_listenSocket.Accept(clientSocket)) {
            didWork = true;
            const Engine::Net::UserId userId = m_nextUserId++;
            auto client = std::make_unique<ConnectedClient>();
            client->userId = userId;
            client->connection = std::make_unique<NetworkConnection>(std::move(clientSocket));
            if (m_tlsConfig.enabled && !client->connection->EnableTlsServer(m_tlsConfig)) {
                LOG_WARNING_FMT("NetServer", "TLS handshake failed for pending user %u", userId);
            } else {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                m_clients[userId] = std::move(client);
                LOG_INFO_FMT("NetServer", "Accepted connection (pending login) user slot %u", userId);
            }
            clientSocket = Socket();
        }

        didWork = didWork || PollClients() > 0;

        {
            const size_t selfFlushBudget =
                (std::max)(size_t{128}, m_scalePolicy.maxPeerTransformsPerTick / 4);
            didWork = FlushPendingSelfTransforms(selfFlushBudget) > 0 || didWork;
        }

        if (GetReplicationQueueDepth() > 0) {
            const size_t ioFlushBudget =
                (std::max)(size_t{256}, m_scalePolicy.maxPeerTransformsPerTick / 8);
            didWork = FlushPendingPeerTransforms(ioFlushBudget) > 0 || didWork;
        }

        if (!didWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

size_t NetServer::PollClients() {

    std::vector<Engine::Net::UserId> disconnected;

    std::vector<std::pair<Engine::Net::UserId, Engine::Net::NetPacket>> messages;



    {

        std::lock_guard<std::mutex> lock(m_clientsMutex);

        for (auto& entry : m_clients) {

            if (!entry.second || !entry.second->connection) {

                continue;

            }



            Engine::Net::NetPacket message;

            while (entry.second->connection->ReceivePacket(message)) {

                messages.emplace_back(entry.first, std::move(message));

            }



            if (!entry.second->connection->IsConnected()) {

                disconnected.push_back(entry.first);

            }

        }

    }



    for (Engine::Net::UserId userId : disconnected) {
        RemoveClient(userId);
    }

    for (auto& entry : messages) {
        HandleClientMessage(entry.first, entry.second);
    }

    return messages.size();
}



void NetServer::RemoveClient(Engine::Net::UserId userId) {

    Engine::Net::PeerEventPayload event;

    event.userId = userId;

    bool wasLoggedIn = false;



    {

        std::lock_guard<std::mutex> lock(m_clientsMutex);

        auto it = m_clients.find(userId);

        if (it == m_clients.end()) {

            return;

        }

        wasLoggedIn = it->second->loggedIn;

        event.displayName = it->second->displayName;

        if (it->second->connection) {

            it->second->connection->Close();

        }

        m_clients.erase(it);

    }



    if (wasLoggedIn) {

        ClearUdpEndpoint(userId);

        ClearClientWorldState(userId);

        Broadcast(Engine::Net::MakeMessage(

                      Engine::Net::MessageType::PeerLeft, event.Serialize(), m_sequence++),

                  userId);

        if (m_clientDisconnectedCallback) {

            m_clientDisconnectedCallback(userId);

        }

    }

    LOG_INFO_FMT("NetServer", "Client %u disconnected", userId);

}



void NetServer::SetShardConfig(const Engine::Net::ShardConfig& config) {
    m_shardConfig = config;
    if (config.maxPlayers > 0) {
        m_scalePolicy.shardHardCap = config.maxPlayers;
    }
}

void NetServer::SetScalePolicyConfig(const Engine::Net::ScalePolicyConfig& config) {
    m_scalePolicy = config;
    m_cellLayerAssigner.SetPolicy(config);
    if (config.shardHardCap > 0) {
        m_shardConfig.maxPlayers = config.shardHardCap;
    }
    if (config.densityCellSize > 0.0f) {
        m_interestManager.SetCellSize(config.densityCellSize);
    }
    if (config.layerFarRadius > 0.0f) {
        m_interestManager.SetDefaultInterestRadius(config.layerFarRadius);
    }
    m_interestManager.SetMaxObserversPerBroadcast(config.observersCapNormal);
}

void NetServer::SetReplicationTickNumber(uint64_t tickNumber) {
    m_replicationTickNumber.store(tickNumber, std::memory_order_release);
}

uint64_t NetServer::GetReplicationTickNumber() const {
    return m_replicationTickNumber.load(std::memory_order_acquire);
}

void NetServer::UpdateCellLayerForPlayer(Engine::Net::UserId userId, const glm::vec3& position) {
    const size_t cellPopulation = m_interestManager.CountUsersInCell(position);
    m_cellLayerAssigner.UpdatePlayer(userId, position, cellPopulation);
}

void NetServer::ApplyZoneScaleConfig(const Engine::Net::ZoneScaleConfig& config) {
    SetShardConfig(config.shard);
    SetScalePolicyConfig(config.scalePolicy);
}

void NetServer::ApplyLayeringState(bool layeringActive) {
    m_layeringActive.store(layeringActive, std::memory_order_release);
    const size_t observerCap = layeringActive ? m_scalePolicy.observersCapCrowded
                                             : m_scalePolicy.observersCapNormal;
    m_interestManager.SetMaxObserversPerBroadcast(observerCap);
    if (layeringActive) {
        m_interestManager.SetDefaultInterestRadius(m_scalePolicy.layerMidRadius);
    } else if (m_scalePolicy.layerFarRadius > 0.0f) {
        m_interestManager.SetDefaultInterestRadius(m_scalePolicy.layerFarRadius);
    }
}

bool NetServer::IsShardAtCapacity() const {

    const uint32_t players = static_cast<uint32_t>(GetLoggedInClientCount());
    const uint32_t overflowPlayers = m_overflowLayerPopulation.load(std::memory_order_relaxed);
    const auto admission = Engine::Net::EvaluateLoginAdmission(
        players,
        overflowPlayers,
        m_shardConfig.HasOverflowTarget(),
        m_scalePolicy,
        GetObservedTickP95Ms());
    return admission == Engine::Net::LayerLoginAdmission::RedirectOverflow ||
           admission == Engine::Net::LayerLoginAdmission::SplitPending ||
           admission == Engine::Net::LayerLoginAdmission::RejectFull;

}



void NetServer::SetOverflowLayerPopulation(uint32_t playerCount) {
    m_overflowLayerPopulation.store(playerCount, std::memory_order_relaxed);
}

uint32_t NetServer::GetOverflowLayerPopulation() const {
    return m_overflowLayerPopulation.load(std::memory_order_relaxed);
}

Engine::Net::LayerSplitPlan NetServer::GetLayerSplitPlan() const {
    const uint32_t players = static_cast<uint32_t>(GetLoggedInClientCount());
    return Engine::Net::PlanLayerSplit(players, m_scalePolicy);
}

void NetServer::FillRedirectResponse(Engine::Net::LoginResponsePayload& response) const {

    response.success = false;

    response.redirect = true;

    response.shardId = m_shardConfig.overflowShardId != 0 ? m_shardConfig.overflowShardId
                                                          : m_shardConfig.shardId;

    response.redirectHost = m_shardConfig.redirectHost;

    response.redirectTcpPort = m_shardConfig.redirectTcpPort;

    response.redirectUdpPort = m_shardConfig.redirectUdpPort;

    response.message = "shard_full";

}

void NetServer::SendLayerMigrationRedirect(Engine::Net::UserId userId) {
    const uint32_t targetShardId = m_shardConfig.overflowShardId != 0 ? m_shardConfig.overflowShardId
                                                                        : m_shardConfig.shardId;
    const uint64_t expiryUnixSeconds =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count()) +
        120;

    Engine::Net::ZoneTransferPayload transfer;
    transfer.userId = userId;
    transfer.shardId = targetShardId;
    transfer.redirectHost = m_shardConfig.redirectHost;
    transfer.redirectTcpPort = m_shardConfig.redirectTcpPort;
    transfer.redirectUdpPort = m_shardConfig.redirectUdpPort;
    transfer.reason = "layer_split";
    if (!m_requiredAuthToken.empty()) {
        transfer.migrationToken = Engine::Net::IssueMigrationToken(
            userId, m_shardConfig.shardId, targetShardId, expiryUnixSeconds, m_requiredAuthToken);
    }

    SendToClient(userId, Engine::Net::MakeMessage(
                             Engine::Net::MessageType::ZoneTransfer,
                             transfer.Serialize(),
                             m_sequence++));

    Engine::Net::LoginResponsePayload response;
    response.assignedUserId = userId;
    FillRedirectResponse(response);
    response.message = "layer_split";
    response.migrationToken = transfer.migrationToken;
    SendToClient(userId, Engine::Net::MakeMessage(
                             Engine::Net::MessageType::LoginResponse,
                             response.Serialize(),
                             m_sequence++));
}

bool NetServer::TryExecuteLayerSplit(const std::function<void(Engine::Net::UserId)>& onPlayerMigrated) {
    if (!m_shardConfig.HasOverflowTarget()) {
        return false;
    }

    const uint32_t localPlayers = static_cast<uint32_t>(GetLoggedInClientCount());
    const uint32_t overflowPlayers = m_overflowLayerPopulation.load(std::memory_order_relaxed);
    const auto admission = Engine::Net::EvaluateLayerLoginAdmission(
        localPlayers,
        overflowPlayers,
        true,
        m_scalePolicy);
    if (admission != Engine::Net::LayerLoginAdmission::SplitPending) {
        return false;
    }

    const Engine::Net::LayerSplitPlan plan = Engine::Net::PlanLayerSplit(localPlayers, m_scalePolicy);
    if (!plan.shouldSplit || plan.migrateCount == 0) {
        return false;
    }

    std::vector<Engine::Net::UserId> migrateIds;
    migrateIds.reserve(static_cast<size_t>(localPlayers));
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (const auto& entry : m_clients) {
            if (entry.second) {
                migrateIds.push_back(entry.first);
            }
        }
    }

    if (migrateIds.empty()) {
        return false;
    }

    std::sort(migrateIds.begin(), migrateIds.end());
    if (migrateIds.size() > plan.migrateCount) {
        migrateIds.resize(plan.migrateCount);
    }

    for (Engine::Net::UserId userId : migrateIds) {
        SendLayerMigrationRedirect(userId);
        if (onPlayerMigrated) {
            onPlayerMigrated(userId);
        }
        RemoveClient(userId);
    }

    LOG_INFO_FMT("NetServer",
                 "Layer split migrated %zu players to overflow shard %u",
                 migrateIds.size(),
                 m_shardConfig.overflowShardId != 0 ? m_shardConfig.overflowShardId
                                                    : m_shardConfig.shardId);
    return true;
}



void NetServer::HandleClientMessage(Engine::Net::UserId userId, const Engine::Net::NetPacket& message) {

    switch (message.GetType()) {

        case Engine::Net::MessageType::LoginRequest: {

            Engine::Net::LoginRequestPayload request;

            if (!Engine::Net::LoginRequestPayload::Deserialize(message.payload, request)) {

                break;

            }

            if (!m_securityPolicy.allowOpenLogin &&
                !Engine::Net::IsAuthenticationConfigured(m_requiredAuthToken, m_credentialStore)) {
                Engine::Net::LoginResponsePayload response;
                response.success = false;
                response.assignedUserId = userId;
                response.message = "open_login_disabled";
                SendToClient(userId,
                             Engine::Net::MakeMessage(Engine::Net::MessageType::LoginResponse,
                                                      response.Serialize(),
                                                      m_sequence++));
                LOG_WARNING_FMT("NetServer", "Rejected login for user %u (open login disabled)", userId);
                break;
            }

            if (!AuthTokenMatches(m_requiredAuthToken, request.authToken)) {

                Engine::Net::LoginResponsePayload response;

                response.success = false;

                response.assignedUserId = userId;

                response.message = "unauthorized";

                SendToClient(userId, Engine::Net::MakeMessage(

                                         Engine::Net::MessageType::LoginResponse,

                                         response.Serialize(),

                                         m_sequence++));

                LOG_WARNING_FMT("NetServer", "Rejected login for user %u (bad auth token)", userId);

                break;

            }



            if (m_credentialStore.RequiresCredentials()) {

                if (request.username.empty() || request.password.empty() ||

                    !m_credentialStore.Verify(request.username, request.password)) {

                    Engine::Net::LoginResponsePayload response;

                    response.success = false;

                    response.assignedUserId = userId;

                    response.message = "invalid_credentials";

                    SendToClient(userId, Engine::Net::MakeMessage(

                                             Engine::Net::MessageType::LoginResponse,

                                             response.Serialize(),

                                             m_sequence++));

                    LOG_WARNING_FMT("NetServer", "Rejected login for user %u (invalid credentials)", userId);

                    break;

                }

            }



            if (!request.migrationToken.empty()) {
                Engine::Net::MigrationTokenPayload migration;
                if (!Engine::Net::ParseMigrationToken(request.migrationToken, migration) ||
                    !Engine::Net::VerifyMigrationToken(
                        migration,
                        m_requiredAuthToken,
                        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                                  std::chrono::system_clock::now().time_since_epoch())
                                                  .count()))) {
                    Engine::Net::LoginResponsePayload response;
                    response.success = false;
                    response.assignedUserId = userId;
                    response.message = "invalid_migration_token";
                    SendToClient(userId, Engine::Net::MakeMessage(
                                             Engine::Net::MessageType::LoginResponse,
                                             response.Serialize(),
                                             m_sequence++));
                    LOG_WARNING_FMT("NetServer", "Rejected migration login for user %u", userId);
                    break;
                }
                request.preferredUserId = migration.userId;
            }



            if (!m_loginRateLimiter.TryConsume(0)) {
                m_loginAttemptsDropped.fetch_add(1, std::memory_order_relaxed);

                Engine::Net::LoginResponsePayload response;
                response.success = false;
                response.assignedUserId = userId;
                response.message = "rate_limited";
                SendToClient(userId, Engine::Net::MakeMessage(
                                         Engine::Net::MessageType::LoginResponse,
                                         response.Serialize(),
                                         m_sequence++));
                LOG_WARNING_FMT("NetServer", "Rejected login for user %u (rate limited)", userId);
                break;
            }

            std::string displayName = SanitizeDisplayName(request.displayName);
            if (displayName.empty()) {
                displayName = "Player" + std::to_string(userId);
            }

            Engine::Net::LoginResponsePayload response;
            response.assignedUserId = userId;
            bool loginAccepted = false;
            bool firstLogin = false;

            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);

                auto it = m_clients.find(userId);
                if (it == m_clients.end()) {
                    return;
                }

                Engine::Net::UserId effectiveUserId = userId;
                if (request.preferredUserId != Engine::Net::kInvalidUserId &&
                    request.preferredUserId != userId) {
                    const auto preferredIt = m_clients.find(request.preferredUserId);
                    bool preferredSlotAvailable = preferredIt == m_clients.end();
                    if (!preferredSlotAvailable && preferredIt->second) {
                        const bool pendingSlot = !preferredIt->second->loggedIn;
                        const bool staleSession =
                            preferredIt->second->loggedIn &&
                            (!preferredIt->second->connection ||
                             !preferredIt->second->connection->IsConnected());
                        preferredSlotAvailable = pendingSlot || staleSession;
                    }
                    if (preferredSlotAvailable) {
                        if (preferredIt != m_clients.end()) {
                            if (preferredIt->second && preferredIt->second->connection) {
                                preferredIt->second->connection->Close();
                            }
                            m_clients.erase(preferredIt);
                        }
                        auto migratedClient = std::move(it->second);
                        m_clients.erase(it);
                        migratedClient->userId = request.preferredUserId;
                        effectiveUserId = request.preferredUserId;
                        m_clients[effectiveUserId] = std::move(migratedClient);
                        it = m_clients.find(effectiveUserId);
                        if (m_nextUserId <= request.preferredUserId) {
                            m_nextUserId = request.preferredUserId + 1;
                        }
                    }
                }

                if (it == m_clients.end() || !it->second) {
                    return;
                }

                userId = effectiveUserId;
                response.assignedUserId = effectiveUserId;

                const uint32_t localPlayers = static_cast<uint32_t>(CountLoggedInClientsUnlocked());
                const uint32_t overflowPlayers = m_overflowLayerPopulation.load(std::memory_order_relaxed);
                const auto admission = Engine::Net::EvaluateLoginAdmission(
                    localPlayers,
                    overflowPlayers,
                    m_shardConfig.HasOverflowTarget(),
                    m_scalePolicy,
                    GetObservedTickP95Ms());

                if (admission != Engine::Net::LayerLoginAdmission::AcceptLocal) {
                    if (admission == Engine::Net::LayerLoginAdmission::RedirectOverflow) {
                        FillRedirectResponse(response);
                    } else if (admission == Engine::Net::LayerLoginAdmission::SplitPending) {
                        response.success = false;
                        response.redirect = false;
                        response.message = "split_pending";
                    } else {
                        response.success = false;
                        response.redirect = false;
                        response.message = "shard_full";
                    }
                } else {
                    firstLogin = !it->second->loggedIn;
                    it->second->displayName = displayName;
                    it->second->udpSessionToken = GenerateSessionToken();
                    it->second->loggedIn = true;
                    loginAccepted = true;
                    response.success = true;
                    response.message = "ok";
                    response.shardId = m_shardConfig.shardId;
                    response.udpSessionToken = it->second->udpSessionToken;
                }
            }

            if (!loginAccepted) {
                SendToClient(userId, Engine::Net::MakeMessage(
                                         Engine::Net::MessageType::LoginResponse,
                                         response.Serialize(),
                                         m_sequence++));

                LOG_WARNING_FMT("NetServer",
                                "Rejected login for user %u (%s)",
                                userId,
                                response.message.c_str());
                break;
            }

            SendToClient(userId, Engine::Net::MakeMessage(
                                     Engine::Net::MessageType::LoginResponse,
                                     response.Serialize(),
                                     m_sequence++));



            if (!m_authoritativeMode && firstLogin) {
                Engine::Net::PeerEventPayload joined;

                joined.userId = userId;

                joined.displayName = displayName;

                Broadcast(Engine::Net::MakeMessage(

                              Engine::Net::MessageType::PeerJoined, joined.Serialize(), m_sequence++),

                          userId);
            }

            LOG_INFO_FMT("NetServer", "Client logged in as %u (%s)%s",
                         userId,
                         displayName.c_str(),
                         firstLogin ? "" : " (re-login)");

            if (firstLogin && m_clientLoggedInCallback) {

                m_clientLoggedInCallback(userId, displayName);

            }

            break;

        }

        case Engine::Net::MessageType::TransformState: {

            if (m_authoritativeMode) {

                break;

            }

            Engine::Net::TransformStatePayload transform;

            if (!Engine::Net::TransformStatePayload::Deserialize(message.payload, transform)) {

                break;

            }

            transform.userId = userId;

            HandleUnreliableTransform(userId, transform);

            break;

        }

        case Engine::Net::MessageType::EntitySnapshot:

        case Engine::Net::MessageType::ComponentSnapshot:

        case Engine::Net::MessageType::WorldSnapshot: {

            if (m_authoritativeMode) {

                break;

            }

            Broadcast(message, userId);

            break;

        }

        case Engine::Net::MessageType::GenericData: {

            Engine::Net::GenericDataPayload generic;

            if (!Engine::Net::GenericDataPayload::Deserialize(message.payload, generic)) {

                break;

            }

            generic.userId = userId;

            if (m_genericDataInterceptor && m_genericDataInterceptor(userId, generic)) {

                break;

            }

            if (m_authoritativeMode) {

                break;

            }

            Broadcast(Engine::Net::MakeMessage(

                          Engine::Net::MessageType::GenericData, generic.Serialize(), m_sequence++),

                      userId);

            break;

        }

        case Engine::Net::MessageType::Logout: {

            RemoveClient(userId);

            break;

        }

        case Engine::Net::MessageType::Ping: {
            Engine::Net::PingPayload ping;
            if (!Engine::Net::PingPayload::Deserialize(message.payload, ping)) {
                break;
            }

            Engine::Net::PongPayload pong;
            pong.sequence = ping.sequence;
            pong.sentTimestampMs = ping.sentTimestampMs;
            pong.serverTimestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                             std::chrono::steady_clock::now().time_since_epoch())
                                                             .count());
            SendToClient(userId,
                         Engine::Net::MakeMessage(Engine::Net::MessageType::Pong,
                                                  pong.Serialize(),
                                                  m_sequence++));
            break;
        }

        default:

            break;

    }

}



void NetServer::HandleUdpMovementCommand(Engine::Net::UserId userId, Engine::Net::MovementCommand& command) {

    if (!m_movementRateLimiter.TryConsume(userId)) {
        m_movementCommandsDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (m_movementCommandInterceptor) {

        m_movementCommandInterceptor(userId, command);

    }

}



void NetServer::HandleUnreliableTransform(Engine::Net::UserId userId,

                                          const Engine::Net::TransformStatePayload& transform) {

    Engine::Net::TransformStatePayload last = transform;

    {

        std::lock_guard<std::mutex> lock(m_worldMutex);

        const auto lastIt = m_lastTransforms.find(userId);

        if (lastIt != m_lastTransforms.end()) {

            last = lastIt->second;

        }

    }



    AuthorityValidationResult validation = m_authority.ValidateTransform(transform, last, 1.0f / 60.0f);

    Engine::Net::TransformStatePayload authoritative = transform;

    authoritative.userId = userId;

    if (!validation.accepted) {

        authoritative.position = validation.correctedPosition;

        authoritative.rotationDegrees = validation.correctedRotationDegrees;

    }



    {

        std::lock_guard<std::mutex> lock(m_worldMutex);

        m_authority.ApplyAuthoritative(userId, authoritative);

        m_lastTransforms[userId] = authoritative;

        m_interestManager.UpdateEntity(userId, authoritative.position);

    }



    const Engine::Net::NetPacket packet = Engine::Net::MakeMessage(

        Engine::Net::MessageType::TransformState, authoritative.Serialize(), m_sequence++);

    BroadcastToInterested(packet, userId, authoritative.position, userId);

    SendToClient(userId, packet);

}



void NetServer::BroadcastToInterested(const Engine::Net::NetPacket& message,

                                      Engine::Net::UserId sourceUserId,

                                      const glm::vec3& sourcePosition,

                                      Engine::Net::UserId excludeUserId) {

    std::vector<Engine::Net::UserId> observers;

    {

        std::lock_guard<std::mutex> lock(m_worldMutex);

        observers = m_interestManager.GetInterestedObservers(sourceUserId, sourcePosition);

    }

    std::vector<NetworkConnection*> connections;
    connections.reserve(observers.size());
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (Engine::Net::UserId observerId : observers) {
            if (observerId == excludeUserId) {
                continue;
            }
            auto it = m_clients.find(observerId);
            if (it == m_clients.end() || !it->second || !it->second->connection) {
                continue;
            }
            connections.push_back(it->second->connection.get());
        }
    }

    for (NetworkConnection* connection : connections) {
        connection->SendPacket(message);
    }

}



bool NetServer::SendToClient(Engine::Net::UserId userId, const Engine::Net::NetPacket& message) {

    std::lock_guard<std::mutex> lock(m_clientsMutex);

    auto it = m_clients.find(userId);

    if (it == m_clients.end() || !it->second || !it->second->connection) {

        return false;

    }

    return it->second->connection->SendPacket(message);

}



bool NetServer::SendUdpPayloadTo(Engine::Net::UserId userId,
                                 Engine::Net::MessageType type,
                                 const Engine::Net::NetBuffer& payload) {
    std::string endpointKey;
    uint64_t sessionToken = 0;
    {
        std::lock_guard<std::mutex> worldLock(m_worldMutex);
        const auto endpointIt = m_userToEndpoint.find(userId);
        if (endpointIt == m_userToEndpoint.end()) {
            return false;
        }
        endpointKey = endpointIt->second;
    }

    std::string host;
    uint16_t port = 0;
    if (!ParseEndpointKey(endpointKey, host, port)) {
        return false;
    }

    sessionToken = GetClientSessionToken(userId);
    if (sessionToken == 0) {
        return false;
    }

    return m_udpChannel.SendToEndpoint(host, port, type, payload, sessionToken);
}

bool NetServer::SendUdpTransformTo(Engine::Net::UserId userId,

                                   const Engine::Net::TransformStatePayload& transform) {

    return SendUdpPayloadTo(userId,
                            Engine::Net::MessageType::UnreliableTransform,
                            transform.Serialize());
}

bool NetServer::SendUdpTransformBatchTo(
    Engine::Net::UserId userId,
    const std::vector<Engine::Net::TransformStatePayload>& transforms) {
    if (transforms.empty()) {
        return true;
    }
    if (transforms.size() == 1) {
        return SendUdpTransformTo(userId, transforms.front());
    }

    Engine::Net::TransformBatchPayload batch;
    batch.transforms = transforms;
    return SendUdpPayloadTo(userId,
                            Engine::Net::MessageType::UnreliableTransformBatch,
                            batch.Serialize());
}

void NetServer::Broadcast(const Engine::Net::NetPacket& message, Engine::Net::UserId excludeUserId) {

    std::lock_guard<std::mutex> lock(m_clientsMutex);

    for (auto& entry : m_clients) {

        if (entry.first == excludeUserId || !entry.second || !entry.second->connection) {

            continue;

        }

        entry.second->connection->SendPacket(message);

    }

}



std::string NetServer::MakeEndpointKey(const std::string& host, uint16_t port) {

    return host + ":" + std::to_string(port);

}



bool NetServer::ParseEndpointKey(const std::string& endpointKey, std::string& outHost, uint16_t& outPort) const {

    const size_t colon = endpointKey.rfind(':');

    if (colon == std::string::npos || colon == 0 || colon + 1 >= endpointKey.size()) {

        return false;

    }

    outHost = endpointKey.substr(0, colon);

    try {

        outPort = static_cast<uint16_t>(std::stoul(endpointKey.substr(colon + 1)));

    } catch (...) {

        return false;

    }

    return true;

}



uint64_t NetServer::GetClientSessionToken(Engine::Net::UserId userId) const {

    std::lock_guard<std::mutex> lock(m_clientsMutex);

    const auto it = m_clients.find(userId);

    if (it == m_clients.end() || !it->second) {

        return 0;

    }

    return it->second->udpSessionToken;

}



void NetServer::HandleUdpSessionRegister(const Engine::Net::UdpSessionRegisterPayload& registration,

                                         const UdpDatagramContext& context) {

    if (registration.userId == Engine::Net::kInvalidUserId || registration.sessionToken == 0) {

        return;

    }



    std::lock_guard<std::mutex> clientsLock(m_clientsMutex);

    const auto clientIt = m_clients.find(registration.userId);

    if (clientIt == m_clients.end() || !clientIt->second) {

        return;

    }

    if (clientIt->second->udpSessionToken != registration.sessionToken) {

        LOG_WARNING_FMT("NetServer", "Rejected UDP register for user %u (bad token)", registration.userId);

        return;

    }



    {

        std::lock_guard<std::mutex> worldLock(m_worldMutex);

        ClearUdpEndpointUnlocked(registration.userId);

        const std::string endpointKey = MakeEndpointKey(context.host, context.port);

        m_endpointToUser[endpointKey] = registration.userId;

        m_userToEndpoint[registration.userId] = endpointKey;

    }

    LOG_INFO_FMT("NetServer", "UDP session bound for user %u at %s:%u",

                 registration.userId, context.host.c_str(), context.port);

}



Engine::Net::UserId NetServer::ResolveUdpUser(const UdpDatagramContext& context) const {

    const std::string endpointKey = MakeEndpointKey(context.host, context.port);

    std::lock_guard<std::mutex> lock(m_worldMutex);

    const auto it = m_endpointToUser.find(endpointKey);

    if (it == m_endpointToUser.end()) {

        return Engine::Net::kInvalidUserId;

    }

    return it->second;

}



void NetServer::ClearUdpEndpoint(Engine::Net::UserId userId) {
    std::lock_guard<std::mutex> lock(m_worldMutex);
    ClearUdpEndpointUnlocked(userId);
}

void NetServer::ClearUdpEndpointUnlocked(Engine::Net::UserId userId) {
    const auto endpointIt = m_userToEndpoint.find(userId);
    if (endpointIt != m_userToEndpoint.end()) {
        m_endpointToUser.erase(endpointIt->second);
        m_userToEndpoint.erase(endpointIt);
    }
}



} // namespace Networking

} // namespace Platform

} // namespace Engine

