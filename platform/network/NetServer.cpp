#include "network/NetServer.h"

#include "Logger.h"

#include <chrono>
#include <random>
#include <thread>
#include <algorithm>
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
    return required.empty() || required == provided;
}

} // namespace

namespace Engine {
namespace Platform {
namespace Networking {

NetServer::NetServer() = default;

NetServer::~NetServer() {
    Stop();
}

bool NetServer::Start(const std::string& host, uint16_t port, uint16_t udpPort) {
    Stop();

    if (!Socket::InitializeNetworking()) {
        return false;
    }

    m_listenSocket = Socket::CreateTcp();
    if (!m_listenSocket.IsValid() ||
        !m_listenSocket.SetReuseAddress(true) ||
        !m_listenSocket.SetNonBlocking(true) ||
        !m_listenSocket.Bind(host, port) ||
        !m_listenSocket.Listen(16)) {
        LOG_ERROR_FMT("NetServer", "Failed to bind/listen on %s:%u", host.c_str(), port);
        m_listenSocket.Close();
        return false;
    }

    if (!m_udpChannel.Bind(host, udpPort)) {
        LOG_WARNING_FMT("NetServer", "UDP bind failed on %s:%u — unreliable movement disabled", host.c_str(), udpPort);
    } else {
        m_udpChannel.SetUnreliableTransformHandler(
            [this](const Engine::Net::TransformStatePayload& transform, const UdpDatagramContext& context) {
                // Authoritative mode ignores client-originated transforms; movement commands only.
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
        m_udpChannel.SetUdpSessionRegisterHandler(
            [this](const Engine::Net::UdpSessionRegisterPayload& registration, const UdpDatagramContext& context) {
                HandleUdpSessionRegister(registration, context);
            });
    }

    m_running = true;
    m_acceptThread = std::thread([this]() { AcceptLoop(); });
    LOG_INFO_FMT("NetServer", "Listening TCP %s:%u UDP %s:%u", host.c_str(), port, host.c_str(), udpPort);
    return true;
}

void NetServer::Stop() {
    m_running = false;
    m_listenSocket.Close();
    m_udpChannel.Close();
    {
        std::lock_guard<std::mutex> lock(m_worldMutex);
        m_lastTransforms.clear();
        m_endpointToUser.clear();
        m_userToEndpoint.clear();
        m_interestManager = InterestManager();
        m_authority = ServerAuthority();
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    std::vector<std::thread> joinThreads;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto& entry : m_clients) {
            if (entry.second && entry.second->connection) {
                entry.second->connection->Close();
            }
            if (entry.second && entry.second->receiveThread.joinable()) {
                joinThreads.push_back(std::move(entry.second->receiveThread));
            }
        }
        m_clients.clear();
    }

    for (auto& thread : joinThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

size_t NetServer::GetClientCount() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clients.size();
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

void NetServer::StoreAuthoritativeTransform(Engine::Net::UserId userId,
                                            const Engine::Net::TransformStatePayload& transform) {
    std::lock_guard<std::mutex> lock(m_worldMutex);
    m_lastTransforms[userId] = transform;
    m_authority.ApplyAuthoritative(userId, transform);
    m_interestManager.UpdateEntity(userId, transform.position);
}

void NetServer::ClearClientWorldState(Engine::Net::UserId userId) {
    std::lock_guard<std::mutex> lock(m_worldMutex);
    m_lastTransforms.erase(userId);
    m_authority.RemoveUser(userId);
    m_interestManager.RemoveEntity(userId);
}

void NetServer::AcceptLoop() {
    while (m_running) {
        m_udpChannel.Poll();
        Socket clientSocket;
        if (!m_listenSocket.Accept(clientSocket)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        const Engine::Net::UserId userId = m_nextUserId++;
        auto client = std::make_unique<ConnectedClient>();
        client->userId = userId;
        client->connection = std::make_unique<NetworkConnection>(std::move(clientSocket));
        if (m_tlsConfig.enabled && !client->connection->EnableTlsServer(m_tlsConfig)) {
            LOG_WARNING_FMT("NetServer", "TLS handshake failed for pending user %u", userId);
            continue;
        }
        client->receiveThread = std::thread([this, userId]() { ClientReceiveLoop(userId); });

        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            m_clients[userId] = std::move(client);
        }

        LOG_INFO_FMT("NetServer", "Accepted connection (pending login) user slot %u", userId);
    }
}

void NetServer::ClientReceiveLoop(Engine::Net::UserId userId) {
    while (m_running) {
        Engine::Net::NetPacket message;
        NetworkConnection* connection = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            auto it = m_clients.find(userId);
            if (it == m_clients.end() || !it->second || !it->second->connection) {
                return;
            }
            connection = it->second->connection.get();
        }

        if (!connection->ReceivePacket(message)) {
            if (!m_running) {
                return;
            }
            if (!connection->IsConnected()) {
                RemoveClient(userId);
                return;
            }
            continue;
        }

        HandleClientMessage(userId, message);
    }
}

void NetServer::RemoveClient(Engine::Net::UserId userId) {
    std::thread joinThread;
    Engine::Net::PeerEventPayload event;
    event.userId = userId;
    bool wasLoggedIn = false;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        auto it = m_clients.find(userId);
        if (it == m_clients.end()) {
            return;
        }
        wasLoggedIn = !it->second->displayName.empty();
        event.displayName = it->second->displayName;
        if (it->second->connection) {
            it->second->connection->Close();
        }
        joinThread = std::move(it->second->receiveThread);
        m_clients.erase(it);
    }

    if (joinThread.joinable()) {
        if (joinThread.get_id() == std::this_thread::get_id()) {
            // ClientReceiveLoop must not join itself — detach and let Stop() reap on shutdown.
            joinThread.detach();
        } else {
            joinThread.join();
        }
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

void NetServer::HandleClientMessage(Engine::Net::UserId userId, const Engine::Net::NetPacket& message) {
    switch (message.GetType()) {
        case Engine::Net::MessageType::LoginRequest: {
            Engine::Net::LoginRequestPayload request;
            if (!Engine::Net::LoginRequestPayload::Deserialize(message.payload, request)) {
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

            std::string displayName = SanitizeDisplayName(request.displayName);
            if (displayName.empty()) {
                displayName = "Player" + std::to_string(userId);
            }

            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                auto it = m_clients.find(userId);
                if (it == m_clients.end()) {
                    return;
                }
                it->second->displayName = displayName;
                it->second->udpSessionToken = GenerateSessionToken();
            }

            Engine::Net::LoginResponsePayload response;
            response.success = true;
            response.assignedUserId = userId;
            response.message = "ok";
            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                auto it = m_clients.find(userId);
                if (it != m_clients.end()) {
                    response.udpSessionToken = it->second->udpSessionToken;
                }
            }
            SendToClient(userId, Engine::Net::MakeMessage(
                                     Engine::Net::MessageType::LoginResponse,
                                     response.Serialize(),
                                     m_sequence++));

            Engine::Net::PeerEventPayload joined;
            joined.userId = userId;
            joined.displayName = displayName;
            Broadcast(Engine::Net::MakeMessage(
                          Engine::Net::MessageType::PeerJoined, joined.Serialize(), m_sequence++),
                      userId);
            SendExistingPeersTo(userId);
            SendExistingPeerTransformsTo(userId);
            LOG_INFO_FMT("NetServer", "Client logged in as %u (%s)", userId, displayName.c_str());
            if (m_clientLoggedInCallback) {
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
            // In authoritative mode only the server publishes snapshots.
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
            // Authoritative servers only accept interceptor-handled channels
            // (movement/loadout); never peer-relay arbitrary GenericData.
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
        default:
            break;
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

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (Engine::Net::UserId observerId : observers) {
        if (observerId == excludeUserId) {
            continue;
        }
        auto it = m_clients.find(observerId);
        if (it == m_clients.end() || !it->second || !it->second->connection) {
            continue;
        }
        it->second->connection->SendPacket(message);
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

void NetServer::Broadcast(const Engine::Net::NetPacket& message, Engine::Net::UserId excludeUserId) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto& entry : m_clients) {
        if (entry.first == excludeUserId || !entry.second || !entry.second->connection) {
            continue;
        }
        entry.second->connection->SendPacket(message);
    }
}

void NetServer::SendExistingPeersTo(Engine::Net::UserId newUserId) {
    std::vector<Engine::Net::PeerEventPayload> peers;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        peers.reserve(m_clients.size());
        for (const auto& entry : m_clients) {
            if (entry.first == newUserId || !entry.second || entry.second->displayName.empty()) {
                continue;
            }
            peers.push_back(Engine::Net::PeerEventPayload{entry.first, entry.second->displayName});
        }
    }

    for (const auto& peer : peers) {
        SendToClient(newUserId, Engine::Net::MakeMessage(
                                    Engine::Net::MessageType::PeerJoined, peer.Serialize(), m_sequence++));
    }
}

void NetServer::SendExistingPeerTransformsTo(Engine::Net::UserId newUserId) {
    std::vector<Engine::Net::TransformStatePayload> transforms;
    {
        std::lock_guard<std::mutex> lock(m_worldMutex);
        transforms.reserve(m_lastTransforms.size());
        for (const auto& entry : m_lastTransforms) {
            if (entry.first == newUserId) {
                continue;
            }
            transforms.push_back(entry.second);
        }
    }

    for (const auto& transform : transforms) {
        SendToClient(newUserId, Engine::Net::MakeMessage(
                                 Engine::Net::MessageType::TransformState,
                                 transform.Serialize(),
                                 m_sequence++));
    }
}

std::string NetServer::MakeEndpointKey(const std::string& host, uint16_t port) {
    return host + ":" + std::to_string(port);
}

void NetServer::HandleUdpSessionRegister(const Engine::Net::UdpSessionRegisterPayload& registration,
                                         const UdpDatagramContext& context) {
    if (registration.userId == Engine::Net::kInvalidUserId || registration.sessionToken == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    const auto clientIt = m_clients.find(registration.userId);
    if (clientIt == m_clients.end() || !clientIt->second) {
        return;
    }
    if (clientIt->second->udpSessionToken != registration.sessionToken) {
        LOG_WARNING_FMT("NetServer", "Rejected UDP register for user %u (bad token)", registration.userId);
        return;
    }

    ClearUdpEndpoint(registration.userId);
    const std::string endpointKey = MakeEndpointKey(context.host, context.port);
    m_endpointToUser[endpointKey] = registration.userId;
    m_userToEndpoint[registration.userId] = endpointKey;
    LOG_INFO_FMT("NetServer", "UDP session bound for user %u at %s", registration.userId, endpointKey.c_str());
}

Engine::Net::UserId NetServer::ResolveUdpUser(const UdpDatagramContext& context) const {
    const std::string endpointKey = MakeEndpointKey(context.host, context.port);
    const auto it = m_endpointToUser.find(endpointKey);
    if (it == m_endpointToUser.end()) {
        return Engine::Net::kInvalidUserId;
    }
    return it->second;
}

void NetServer::ClearUdpEndpoint(Engine::Net::UserId userId) {
    const auto endpointIt = m_userToEndpoint.find(userId);
    if (endpointIt != m_userToEndpoint.end()) {
        m_endpointToUser.erase(endpointIt->second);
        m_userToEndpoint.erase(endpointIt);
    }
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
