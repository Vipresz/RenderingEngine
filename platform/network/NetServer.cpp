#include "network/NetServer.h"



#include "Logger.h"

#include "core/network/SecureCompare.h"



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

    if (required.empty()) {

        return true;

    }

    return Engine::Net::SecureCompare(required, provided);

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



    m_running = true;

    m_ioThread = std::thread([this]() { IoLoop(); });

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

    size_t count = 0;

    for (const auto& entry : m_clients) {

        if (entry.second && !entry.second->displayName.empty()) {

            ++count;

        }

    }

    return count;

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



void NetServer::SendTransformToInterested(const Engine::Net::TransformStatePayload& transform,

                                            Engine::Net::UserId sourceUserId,

                                            Engine::Net::UserId excludeUserId) {

    std::vector<Engine::Net::UserId> observers;

    {

        std::lock_guard<std::mutex> lock(m_worldMutex);

        observers = m_interestManager.GetInterestedObservers(sourceUserId, transform.position);

    }



    observers.push_back(sourceUserId);
    std::sort(observers.begin(), observers.end());
    observers.erase(std::unique(observers.begin(), observers.end()), observers.end());

    for (Engine::Net::UserId observerId : observers) {

        if (observerId == excludeUserId) {

            continue;

        }

        if (!SendUdpTransformTo(observerId, transform)) {

            SendToClient(observerId, Engine::Net::MakeMessage(

                                         Engine::Net::MessageType::TransformState,

                                         transform.Serialize(),

                                         m_sequence++));

        }

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

}



void NetServer::ClearClientWorldState(Engine::Net::UserId userId) {

    std::lock_guard<std::mutex> lock(m_worldMutex);

    m_lastTransforms.erase(userId);

    m_authority.RemoveUser(userId);

    m_interestManager.RemoveEntity(userId);

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



    for (auto& entry : messages) {

        HandleClientMessage(entry.first, entry.second);

    }



    for (Engine::Net::UserId userId : disconnected) {

        RemoveClient(userId);

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

        wasLoggedIn = !it->second->displayName.empty();

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



bool NetServer::IsShardAtCapacity() const {

    if (m_shardConfig.maxPlayers == 0) {

        return false;

    }

    return GetLoggedInClientCount() >= m_shardConfig.maxPlayers;

}



void NetServer::FillRedirectResponse(Engine::Net::LoginResponsePayload& response) const {

    response.success = false;

    response.redirect = true;

    response.shardId = m_shardConfig.shardId;

    response.redirectHost = m_shardConfig.redirectHost;

    response.redirectTcpPort = m_shardConfig.redirectTcpPort;

    response.redirectUdpPort = m_shardConfig.redirectUdpPort;

    response.message = "shard_full";

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



            if (IsShardAtCapacity()) {

                Engine::Net::LoginResponsePayload response;

                response.assignedUserId = userId;

                FillRedirectResponse(response);

                SendToClient(userId, Engine::Net::MakeMessage(

                                         Engine::Net::MessageType::LoginResponse,

                                         response.Serialize(),

                                         m_sequence++));

                LOG_WARNING_FMT("NetServer", "Rejected login for user %u (shard at capacity)", userId);

                break;

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

            response.shardId = m_shardConfig.shardId;

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



            if (!m_authoritativeMode) {
                Engine::Net::PeerEventPayload joined;

                joined.userId = userId;

                joined.displayName = displayName;

                Broadcast(Engine::Net::MakeMessage(

                              Engine::Net::MessageType::PeerJoined, joined.Serialize(), m_sequence++),

                          userId);
            }

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



bool NetServer::SendUdpTransformTo(Engine::Net::UserId userId,

                                   const Engine::Net::TransformStatePayload& transform) {

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



    return m_udpChannel.SendToEndpoint(host,

                                       port,

                                       Engine::Net::MessageType::UnreliableTransform,

                                       transform.Serialize(),

                                       sessionToken);

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

