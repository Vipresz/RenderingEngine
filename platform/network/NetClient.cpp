#include "network/NetClient.h"

#include "Logger.h"
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

    m_udpChannel.SetUnreliableTransformHandler([this](const Engine::Net::TransformStatePayload& transform) {
        if (m_callbacks.onTransformState) {
            m_callbacks.onTransformState(transform);
        }
    });
    return true;
}

bool NetClient::Login(const std::string& displayName, Engine::Net::UserId preferredUserId) {
    if (!m_connected || !m_connection) {
        return false;
    }

    Engine::Net::LoginRequestPayload request;
    request.displayName = displayName;
    request.preferredUserId = preferredUserId;
    const Engine::Net::NetPacket message =
        Engine::Net::MakeMessage(Engine::Net::MessageType::LoginRequest, request.Serialize(), m_sequence++);

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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_ERROR("NetClient", "Login timed out");
    return false;
}

void NetClient::Disconnect() {
    m_running = false;
    m_connected = false;
    m_loggedIn = false;
    m_userId = Engine::Net::kInvalidUserId;

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
            } else {
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
        case Engine::Net::MessageType::TransformState: {
            Engine::Net::TransformStatePayload transform;
            if (Engine::Net::TransformStatePayload::Deserialize(message.payload, transform) &&
                m_callbacks.onTransformState) {
                m_callbacks.onTransformState(transform);
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
