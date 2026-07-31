#include "network/NetClient.h"

#include "Logger.h"
#include "core/network/SecureSecret.h"
#include "core/network/ReplicationTypes.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif

#include <chrono>
#include <thread>

namespace Engine {
namespace Platform {
namespace Networking {

NetClient::NetClient() = default;

NetClient::~NetClient() {
    Disconnect();
}

bool NetClient::Connect(const std::string& host, uint16_t port) {
    Disconnect();

    if (!Socket::InitializeNetworking()) {
        return false;
    }

    Socket socket = Socket::CreateTcp();
    if (!socket.IsValid() || !socket.Connect(host, port)) {
#ifdef _WIN32
        LOG_ERROR_FMT("NetClient", "Failed to connect to %s:%u (WSA error %d)", host.c_str(), port, WSAGetLastError());
#else
        LOG_ERROR_FMT("NetClient", "Failed to connect to %s:%u", host.c_str(), port);
#endif
        return false;
    }

    m_connection = std::make_unique<NetworkConnection>(std::move(socket));
    if (m_tlsConfig.enabled &&
        !m_connection->EnableTlsClient(m_tlsConfig)) {
        LOG_ERROR("NetClient", "TLS handshake failed");
        m_connection.reset();
        return false;
    }

    m_host = host;
    m_running = true;
    m_connected = true;
    m_receiveThread = std::thread([this]() { ReceiveLoop(); });
    LOG_INFO_FMT("NetClient", "Connected to %s:%u", host.c_str(), port);
    return true;
}

bool NetClient::ConnectUdp(const std::string& host, uint16_t udpPort) {
    m_udpPort = udpPort;
    if (!m_udpChannel.Connect(host, udpPort)) {
        LOG_WARNING_FMT("NetClient", "UDP connect failed for %s:%u (TCP still active)", host.c_str(), udpPort);
        return false;
    }

    m_udpChannel.SetUnreliableTransformHandler(
        [this](const Engine::Net::TransformStatePayload& transform, const UdpDatagramContext&) {
            if (m_callbacks.onTransformState) {
                m_callbacks.onTransformState(transform);
            }
        });
    m_udpChannel.SetUdpMovementAckHandler(
        [this](const Engine::Net::MovementAckPayload& ack, const UdpDatagramContext&) {
            if (m_callbacks.onMovementAck) {
                m_callbacks.onMovementAck(ack);
            }
        });
    return true;
}

bool NetClient::Login(const std::string& displayName,
                      Engine::Net::UserId preferredUserId,
                      const std::string& username,
                      const std::string& password) {
    constexpr int kMaxRedirectHops = 2;
    for (int redirectHop = 0; redirectHop <= kMaxRedirectHops; ++redirectHop) {
        if (!m_connected || !m_connection) {
            return false;
        }

        m_loginRejected = false;
        m_loginRedirectPending = false;
        m_lastLoginDisplayName = displayName;

        Engine::Net::LoginRequestPayload request;
        request.displayName = displayName;
        request.preferredUserId = preferredUserId;
        request.username = username.empty() ? m_username : username;
        request.password = password.empty() ? m_password : password;
        request.authToken = m_authToken;
        const Engine::Net::NetPacket message =
            Engine::Net::MakeMessage(Engine::Net::MessageType::LoginRequest, request.Serialize(), m_sequence++);

        // Use credentials only for this request; do not retain password bytes afterward.
        Engine::Net::SecureZeroString(request.password);

        if (!SendPacket(message)) {
            return false;
        }

        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
            Poll();
            if (m_loggedIn) {
                LOG_INFO_FMT("NetClient", "Logged in as user %u", m_userId);
                return true;
            }
            if (m_loginRedirectPending) {
                break;
            }
            if (m_loginRejected) {
                LOG_ERROR("NetClient", "Login rejected by server");
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!m_loginRedirectPending) {
            LOG_ERROR("NetClient", "Login timed out");
            return false;
        }

        const std::string redirectHost = m_pendingRedirectHost;
        const uint16_t redirectTcpPort = m_pendingRedirectTcpPort;
        const uint16_t redirectUdpPort = m_pendingRedirectUdpPort;
        const std::string migrationToken = m_pendingMigrationToken;
        const Engine::Net::UserId redirectPreferredUserId =
            preferredUserId != Engine::Net::kInvalidUserId ? preferredUserId : m_userId;
        m_loginRedirectPending = false;
        m_pendingRedirectHost.clear();
        m_pendingRedirectTcpPort = 0;
        m_pendingRedirectUdpPort = 0;
        m_pendingMigrationToken.clear();

        if (!FollowRedirect(redirectHost, redirectTcpPort, redirectUdpPort, redirectPreferredUserId, migrationToken)) {
            return false;
        }
    }

    LOG_ERROR("NetClient", "Login redirect hop limit exceeded");
    return false;
}

bool NetClient::FollowRedirect(const std::string& host,
                               uint16_t tcpPort,
                               uint16_t udpPort,
                               Engine::Net::UserId preferredUserId,
                               const std::string& migrationToken) {
    const Engine::Net::UserId preservedUserId = m_userId;
    const std::string displayName = m_lastLoginDisplayName;
    Disconnect();
    if (!Connect(host, tcpPort)) {
        LOG_ERROR_FMT("NetClient", "Failed to follow redirect to %s:%u", host.c_str(), tcpPort);
        return false;
    }
    m_udpPort = udpPort != 0 ? udpPort : static_cast<uint16_t>(tcpPort + 1);

    Engine::Net::LoginRequestPayload request;
    request.displayName = displayName.empty() ? "Player" : displayName;
    request.preferredUserId = preferredUserId != Engine::Net::kInvalidUserId ? preferredUserId
                                                                            : preservedUserId;
    request.username = m_username;
    request.password = m_password;
    request.authToken = m_authToken;
    request.migrationToken = migrationToken;
    const Engine::Net::NetPacket message =
        Engine::Net::MakeMessage(Engine::Net::MessageType::LoginRequest, request.Serialize(), m_sequence++);
    Engine::Net::SecureZeroString(request.password);
    if (!SendPacket(message)) {
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        Poll();
        if (m_loggedIn) {
            LOG_INFO_FMT("NetClient", "Redirect login succeeded as user %u", m_userId);
            return true;
        }
        if (m_loginRejected) {
            LOG_ERROR("NetClient", "Redirect login rejected by server");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_ERROR("NetClient", "Redirect login timed out");
    return false;
}

bool NetClient::ProcessPendingZoneTransfer() {
    if (!m_zoneTransferPending.load(std::memory_order_acquire)) {
        return false;
    }

    const std::string redirectHost = m_pendingRedirectHost;
    const uint16_t redirectTcpPort = m_pendingRedirectTcpPort;
    const uint16_t redirectUdpPort = m_pendingRedirectUdpPort;
    const std::string migrationToken = m_pendingMigrationToken;
    const Engine::Net::UserId preferredUserId = m_userId;

    m_zoneTransferPending.store(false, std::memory_order_release);
    m_pendingRedirectHost.clear();
    m_pendingRedirectTcpPort = 0;
    m_pendingRedirectUdpPort = 0;
    m_pendingMigrationToken.clear();

    return FollowRedirect(redirectHost, redirectTcpPort, redirectUdpPort, preferredUserId, migrationToken);
}

bool NetClient::RegisterUdpSession() {
    if (!m_loggedIn || m_udpSessionToken == 0) {
        return false;
    }
    if (!m_udpChannel.IsOpen()) {
        if (!ConnectUdp(m_host, m_udpPort)) {
            return false;
        }
    }

    Engine::Net::UdpSessionRegisterPayload registration;
    registration.userId = m_userId;
    registration.sessionToken = m_udpSessionToken;
    return m_udpChannel.SendUdpSessionRegister(registration);
}

void NetClient::Disconnect() {
    m_running = false;
    m_connected = false;
    m_loggedIn = false;
    m_userId = Engine::Net::kInvalidUserId;
    m_udpSessionToken = 0;
    m_udpChannel.SetSessionToken(0);
    m_udpChannel.SetEncryptPayloads(false);
    Engine::Net::SecureZeroString(m_password);

    if (m_connection) {
        m_connection->Close();
    }

    m_incoming.Stop();

    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

    m_connection.reset();
    m_incoming.Reset();
    m_udpChannel.Close();
}

bool NetClient::SendTransformState(const Engine::Net::TransformStatePayload& payload) {
    if (!m_loggedIn) {
        return false;
    }
    Engine::Net::TransformStatePayload outgoing = payload;
    outgoing.userId = m_userId;
    if (m_useUdpForMovement && m_udpChannel.IsOpen()) {
        return SendUnreliableTransform(outgoing);
    }
    return SendPacket(Engine::Net::MakeMessage(
        Engine::Net::MessageType::TransformState, outgoing.Serialize(), m_sequence++));
}

bool NetClient::SendUnreliableTransform(const Engine::Net::TransformStatePayload& payload) {
    if (!m_loggedIn || !m_udpChannel.IsOpen()) {
        return false;
    }
    Engine::Net::TransformStatePayload outgoing = payload;
    outgoing.userId = m_userId;
    return m_udpChannel.SendUnreliableTransform(outgoing);
}

bool NetClient::SendMovementCommand(const Engine::Net::MovementCommand& command) {
    if (!m_loggedIn) {
        return false;
    }
    if (m_useUdpForMovement && m_udpChannel.IsOpen()) {
        return m_udpChannel.SendMovementCommand(command);
    }

    Engine::Net::GenericDataPayload payload;
    payload.userId = m_userId;
    payload.tick = command.tick;
    payload.channel = Engine::Net::kMovementCommandChannel;
    const Engine::Net::NetBuffer buffer = Engine::Net::SerializeMovementCommand(command);
    payload.data = buffer.Data();
    return SendGenericData(payload);
}

bool NetClient::SendGenericData(const Engine::Net::GenericDataPayload& payload) {
    if (!m_loggedIn) {
        return false;
    }
    Engine::Net::GenericDataPayload outgoing = payload;
    outgoing.userId = m_userId;
    return SendPacket(Engine::Net::MakeMessage(
        Engine::Net::MessageType::GenericData, outgoing.Serialize(), m_sequence++));
}

bool NetClient::SendEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot) {
    if (!m_loggedIn) {
        return false;
    }
    return SendPacket(Engine::Net::MakeMessage(
        Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), m_sequence++));
}

bool NetClient::SendWorldSnapshot(const Engine::Net::WorldSnapshotPayload& snapshot) {
    if (!m_loggedIn) {
        return false;
    }
    return SendPacket(Engine::Net::MakeMessage(
        Engine::Net::MessageType::WorldSnapshot, snapshot.Serialize(), m_sequence++));
}

bool NetClient::SendPing() {
    if (!m_loggedIn) {
        return false;
    }

    Engine::Net::PingPayload ping;
    ping.sequence = ++m_pingSequence;
    ping.sentTimestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                     std::chrono::steady_clock::now().time_since_epoch())
                                                     .count());
    return SendPacket(Engine::Net::MakeMessage(
        Engine::Net::MessageType::Ping, ping.Serialize(), m_sequence++));
}

void NetClient::Poll() {
    m_udpChannel.Poll();
    Engine::Net::NetPacket message;
    while (m_incoming.TryPop(message)) {
        HandlePacket(message);
    }
}

void NetClient::ReceiveLoop() {
    while (m_running && m_connection && m_connection->IsConnected()) {
        Engine::Net::NetPacket message;
        if (!m_connection->ReceivePacket(message)) {
            if (!m_running) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        m_incoming.Push(std::move(message));
    }

    m_connected = false;
    m_loggedIn = false;
}

void NetClient::HandlePacket(const Engine::Net::NetPacket& message) {
    switch (message.GetType()) {
        case Engine::Net::MessageType::LoginResponse: {
            Engine::Net::LoginResponsePayload response;
            if (!Engine::Net::LoginResponsePayload::Deserialize(message.payload, response)) {
                break;
            }
            if (response.success) {
                m_loggedIn = true;
                m_userId = response.assignedUserId;
                m_udpSessionToken = response.udpSessionToken;
                m_udpChannel.SetSessionToken(m_udpSessionToken);
                Engine::Net::SecureZeroString(m_password);
                if (!m_authToken.empty() && m_udpSessionToken != 0) {
                    m_udpChannel.SetEncryptPayloads(true);
                    m_udpChannel.SetAuthTokenForEncryption(m_authToken);
                }
            } else if (response.redirect && !response.redirectHost.empty() && response.redirectTcpPort != 0) {
                m_pendingRedirectHost = response.redirectHost;
                m_pendingRedirectTcpPort = response.redirectTcpPort;
                m_pendingRedirectUdpPort = response.redirectUdpPort;
                m_pendingMigrationToken = response.migrationToken;
                if (m_loggedIn) {
                    m_zoneTransferPending.store(true, std::memory_order_release);
                } else {
                    m_loginRedirectPending = true;
                }
                LOG_INFO_FMT("NetClient",
                             "Login redirect to shard %u at %s:%u",
                             response.shardId,
                             response.redirectHost.c_str(),
                             response.redirectTcpPort);
            } else {
                m_loginRejected = true;
                LOG_ERROR_FMT("NetClient", "Login failed: %s", response.message.c_str());
            }
            break;
        }
        case Engine::Net::MessageType::PeerJoined: {
            Engine::Net::PeerEventPayload event;
            if (Engine::Net::PeerEventPayload::Deserialize(message.payload, event) && m_callbacks.onPeerJoined) {
                m_callbacks.onPeerJoined(event.userId, event.displayName);
            }
            break;
        }
        case Engine::Net::MessageType::PeerLeft: {
            Engine::Net::PeerEventPayload event;
            if (Engine::Net::PeerEventPayload::Deserialize(message.payload, event) && m_callbacks.onPeerLeft) {
                m_callbacks.onPeerLeft(event.userId);
            }
            break;
        }
        case Engine::Net::MessageType::ZoneTransfer: {
            Engine::Net::ZoneTransferPayload transfer;
            if (!Engine::Net::ZoneTransferPayload::Deserialize(message.payload, transfer)) {
                break;
            }
            m_pendingRedirectHost = transfer.redirectHost;
            m_pendingRedirectTcpPort = transfer.redirectTcpPort;
            m_pendingRedirectUdpPort = transfer.redirectUdpPort;
            m_pendingMigrationToken = transfer.migrationToken;
            m_zoneTransferPending.store(true, std::memory_order_release);
            if (m_callbacks.onZoneTransfer) {
                m_callbacks.onZoneTransfer(transfer);
            }
            LOG_INFO_FMT("NetClient",
                         "Zone transfer requested to shard %u at %s:%u (%s)",
                         transfer.shardId,
                         transfer.redirectHost.c_str(),
                         transfer.redirectTcpPort,
                         transfer.reason.c_str());
            break;
        }
        case Engine::Net::MessageType::TransformState: {
            Engine::Net::TransformStatePayload transform;
            if (Engine::Net::TransformStatePayload::Deserialize(message.payload, transform) &&
                m_callbacks.onTransformState) {
                m_callbacks.onTransformState(transform);
            }
            break;
        }
        case Engine::Net::MessageType::MovementAck: {
            Engine::Net::MovementAckPayload ack;
            if (Engine::Net::MovementAckPayload::Deserialize(message.payload, ack) &&
                m_callbacks.onMovementAck) {
                m_callbacks.onMovementAck(ack);
            }
            break;
        }
        case Engine::Net::MessageType::GenericData: {
            Engine::Net::GenericDataPayload generic;
            if (Engine::Net::GenericDataPayload::Deserialize(message.payload, generic) &&
                m_callbacks.onGenericData) {
                m_callbacks.onGenericData(generic);
            }
            break;
        }
        case Engine::Net::MessageType::WorldSnapshot: {
            Engine::Net::WorldSnapshotPayload snapshot;
            if (Engine::Net::WorldSnapshotPayload::Deserialize(message.payload, snapshot) &&
                m_callbacks.onWorldSnapshot) {
                m_callbacks.onWorldSnapshot(snapshot);
            }
            break;
        }
        case Engine::Net::MessageType::EntitySnapshot: {
            Engine::Net::EntitySnapshot snapshot;
            if (Engine::Net::EntitySnapshot::Deserialize(message.payload, snapshot) &&
                m_callbacks.onEntitySnapshot) {
                m_callbacks.onEntitySnapshot(snapshot);
            }
            break;
        }
        case Engine::Net::MessageType::ComponentSnapshot: {
            Engine::Net::ComponentSnapshot snapshot;
            if (Engine::Net::ComponentSnapshot::Deserialize(message.payload, snapshot) &&
                m_callbacks.onComponentSnapshot) {
                m_callbacks.onComponentSnapshot(snapshot);
            }
            break;
        }
        case Engine::Net::MessageType::Pong: {
            Engine::Net::PongPayload pong;
            if (!Engine::Net::PongPayload::Deserialize(message.payload, pong)) {
                break;
            }
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            if (nowMs >= pong.sentTimestampMs) {
                m_roundTripTimeMs.store(static_cast<float>(nowMs - pong.sentTimestampMs),
                                        std::memory_order_relaxed);
            }
            break;
        }
        default:
            break;
    }
}

bool NetClient::SendPacket(const Engine::Net::NetPacket& message) {
    return m_connection && m_connection->SendPacket(message);
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
