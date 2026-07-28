#include "network/UdpChannel.h"

#include "Logger.h"
#include "core/network/UdpMessageAuth.h"

#include <cstring>
#include <cstddef>

namespace Engine {
namespace Platform {
namespace Networking {

bool UdpChannel::Bind(const std::string& host, uint16_t port) {
    m_socket = DatagramSocket::CreateUdp();
    if (!m_socket.IsValid()) {
        return false;
    }
    m_socket.SetReuseAddress(true);
    m_socket.SetNonBlocking(true);
    if (!m_socket.Bind(host, port)) {
        m_socket.Close();
        return false;
    }
    m_connected = false;
    LOG_INFO_FMT("UdpChannel", "Bound UDP %s:%u", host.empty() ? "0.0.0.0" : host.c_str(), port);
    return true;
}

bool UdpChannel::Connect(const std::string& host, uint16_t port) {
    m_socket = DatagramSocket::CreateUdp();
    if (!m_socket.IsValid()) {
        return false;
    }
    m_socket.SetNonBlocking(true);
    if (!m_socket.Connect(host, port)) {
        m_socket.Close();
        return false;
    }
    m_remoteHost = host;
    m_remotePort = port;
    m_connected = true;
    LOG_INFO_FMT("UdpChannel", "Connected UDP to %s:%u", host.c_str(), port);
    return true;
}

void UdpChannel::Close() {
    m_socket.Close();
    m_connected = false;
    m_remoteHost.clear();
    m_remotePort = 0;
}

bool UdpChannel::SendUnreliableTransform(const Engine::Net::TransformStatePayload& payload) {
    return SendPayload(Engine::Net::MessageType::UnreliableTransform, payload.Serialize());
}

bool UdpChannel::SendMovementCommand(const Engine::Net::MovementCommand& command) {
    return SendPayload(Engine::Net::MessageType::UdpMovementCommand,
                       Engine::Net::SerializeMovementCommand(command));
}

bool UdpChannel::SendUdpSessionRegister(const Engine::Net::UdpSessionRegisterPayload& payload) {
    return SendPayload(Engine::Net::MessageType::UdpSessionRegister, payload.Serialize());
}

bool UdpChannel::SendToEndpoint(const std::string& host,
                                uint16_t port,
                                Engine::Net::MessageType type,
                                const Engine::Net::NetBuffer& payload,
                                uint64_t sessionTokenForAuth) {
    return SendPayloadTo(type, payload, host, port, sessionTokenForAuth);
}

bool UdpChannel::SendPayload(Engine::Net::MessageType type, const Engine::Net::NetBuffer& payload) {
    if (m_connected) {
        return SendPayloadTo(type, payload, m_remoteHost, m_remotePort, m_sessionToken);
    }
    return SendPayloadTo(type, payload, "", 0, m_sessionToken);
}

bool UdpChannel::SendPayloadTo(Engine::Net::MessageType type,
                               const Engine::Net::NetBuffer& payload,
                               const std::string& host,
                               uint16_t port,
                               uint64_t sessionTokenForAuth) {
    if (!m_socket.IsValid()) {
        return false;
    }

    UdpPacketHeader header;
    header.type = static_cast<uint16_t>(type);
    header.sequence = m_sendSequence++;

    const size_t payloadOffset = sizeof(UdpPacketHeader);
    std::vector<uint8_t> packet(payloadOffset + payload.Size());
    std::memcpy(packet.data(), &header, sizeof(UdpPacketHeader));
    if (!payload.Data().empty()) {
        std::memcpy(packet.data() + payloadOffset, payload.Data().data(), payload.Data().size());
    }

    if (sessionTokenForAuth != 0) {
        header.authTag = Engine::Net::ComputeUdpAuthTag(
            sessionTokenForAuth,
            &header,
            offsetof(UdpPacketHeader, authTag),
            packet.data() + payloadOffset,
            payload.Size());
        std::memcpy(packet.data(), &header, sizeof(UdpPacketHeader));
    }

    const SocketResult result = m_connected
        ? m_socket.SendTo(packet.data(), packet.size(), m_remoteHost, m_remotePort)
        : m_socket.SendTo(packet.data(), packet.size(), host, port);
    return result == SocketResult::Ok || result == SocketResult::WouldBlock;
}

void UdpChannel::Poll() {
    PollAndDrain();
}

size_t UdpChannel::PollAndDrain() {
    if (!m_socket.IsValid()) {
        return 0;
    }

    size_t processed = 0;
    uint8_t buffer[2048];
    size_t received = 0;
    std::string host;
    uint16_t port = 0;
    while (true) {
        const SocketResult result = m_socket.RecvFrom(buffer, sizeof(buffer), received, host, port);
        if (result == SocketResult::WouldBlock) {
            break;
        }
        if (result != SocketResult::Ok || received == 0) {
            break;
        }
        HandleDatagram(buffer, received, host, port);
        ++processed;
    }
    return processed;
}

void UdpChannel::HandleDatagram(const uint8_t* data,
                                size_t size,
                                const std::string& host,
                                uint16_t port) {
    if (!data || size < sizeof(UdpPacketHeader)) {
        return;
    }

    UdpPacketHeader header;
    std::memcpy(&header, data, sizeof(UdpPacketHeader));
    if (header.magic != Engine::Net::kUdpChannelMagic ||
        header.version != Engine::Net::kProtocolVersion) {
        return;
    }

    const size_t payloadOffset = sizeof(UdpPacketHeader);
    const size_t payloadSize = size - payloadOffset;
    Engine::Net::NetBuffer payload(std::vector<uint8_t>(data + payloadOffset, data + payloadOffset + payloadSize));

    const UdpDatagramContext context{host, port};

    uint64_t sessionToken = m_sessionToken;
    if (sessionToken == 0 && m_sessionTokenResolver) {
        sessionToken = m_sessionTokenResolver(context);
    }
    if (sessionToken == 0 &&
        static_cast<Engine::Net::MessageType>(header.type) == Engine::Net::MessageType::UdpSessionRegister) {
        Engine::Net::UdpSessionRegisterPayload registration;
        if (Engine::Net::UdpSessionRegisterPayload::Deserialize(payload, registration)) {
            sessionToken = registration.sessionToken;
        }
    }

    if (m_requireAuthenticatedDatagrams) {
        if (sessionToken == 0 || header.authTag == 0) {
            return;
        }
        if (!Engine::Net::VerifyUdpAuthTag(sessionToken,
                                          header.authTag,
                                          &header,
                                          offsetof(UdpPacketHeader, authTag),
                                          payload.Data().data(),
                                          payload.Data().size())) {
            LOG_WARNING("UdpChannel", "Rejected UDP datagram with invalid auth tag");
            return;
        }
    }

    switch (static_cast<Engine::Net::MessageType>(header.type)) {
        case Engine::Net::MessageType::UnreliableTransform: {
            Engine::Net::TransformStatePayload transform;
            if (Engine::Net::TransformStatePayload::Deserialize(payload, transform) && m_transformHandler) {
                m_transformHandler(transform, context);
            }
            break;
        }
        case Engine::Net::MessageType::UdpSessionRegister: {
            Engine::Net::UdpSessionRegisterPayload registration;
            if (Engine::Net::UdpSessionRegisterPayload::Deserialize(payload, registration) && m_registerHandler) {
                m_registerHandler(registration, context);
            }
            break;
        }
        case Engine::Net::MessageType::UdpMovementCommand: {
            Engine::Net::MovementCommand command;
            if (Engine::Net::DeserializeMovementCommand(payload, command) && m_movementHandler) {
                m_movementHandler(command, context);
            }
            break;
        }
        case Engine::Net::MessageType::MovementAck: {
            Engine::Net::MovementAckPayload ack;
            if (Engine::Net::MovementAckPayload::Deserialize(payload, ack) && m_movementAckHandler) {
                m_movementAckHandler(ack, context);
            }
            break;
        }
        default:
            break;
    }
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
