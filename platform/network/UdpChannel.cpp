#include "network/UdpChannel.h"

#include "Logger.h"

#include <cstring>

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

bool UdpChannel::SendPayload(Engine::Net::MessageType type, const Engine::Net::NetBuffer& payload) {
    if (!m_socket.IsValid()) {
        return false;
    }

    UdpPacketHeader header;
    header.type = static_cast<uint16_t>(type);
    header.sequence = m_sendSequence++;

    std::vector<uint8_t> packet(sizeof(UdpPacketHeader) + payload.Size());
    std::memcpy(packet.data(), &header, sizeof(UdpPacketHeader));
    if (!payload.Data().empty()) {
        std::memcpy(packet.data() + sizeof(UdpPacketHeader), payload.Data().data(), payload.Data().size());
    }

    const SocketResult result = m_connected
        ? m_socket.SendTo(packet.data(), packet.size(), m_remoteHost, m_remotePort)
        : m_socket.SendTo(packet.data(), packet.size(), "", 0);
    return result == SocketResult::Ok || result == SocketResult::WouldBlock;
}

void UdpChannel::Poll() {
    if (!m_socket.IsValid()) {
        return;
    }

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
        HandleDatagram(buffer, received);
    }
}

void UdpChannel::HandleDatagram(const uint8_t* data, size_t size) {
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

    switch (static_cast<Engine::Net::MessageType>(header.type)) {
        case Engine::Net::MessageType::UnreliableTransform: {
            Engine::Net::TransformStatePayload transform;
            if (Engine::Net::TransformStatePayload::Deserialize(payload, transform) && m_transformHandler) {
                m_transformHandler(transform);
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
