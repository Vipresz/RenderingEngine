#include "network/NetworkConnection.h"

#include "Logger.h"

namespace Engine {
namespace Platform {
namespace Networking {

NetworkConnection::NetworkConnection(Socket socket) : m_socket(std::move(socket)) {}

bool NetworkConnection::EnableTlsClient(const TlsConfig& config) {
    if (!m_socket.IsValid() || !config.enabled) {
        return false;
    }

    m_tls = std::make_unique<TlsSession>();
    if (!m_tls->InitializeClient(m_socket.NativeHandle(), config) || !m_tls->Handshake()) {
        m_tls.reset();
        return false;
    }
    return true;
}

bool NetworkConnection::EnableTlsServer(const TlsConfig& config) {
    if (!m_socket.IsValid() || !config.enabled) {
        return false;
    }

    m_tls = std::make_unique<TlsSession>();
    if (!m_tls->InitializeServer(m_socket.NativeHandle(), config) || !m_tls->Handshake()) {
        m_tls.reset();
        return false;
    }
    return true;
}

void NetworkConnection::Close() {
    if (m_tls) {
        m_tls->Close();
        m_tls.reset();
    }
    m_socket.Close();
    m_readBuffer.clear();
}

bool NetworkConnection::WriteExact(const uint8_t* data, size_t size) {
    if (!IsConnected()) {
        return false;
    }
    if (m_tls && m_tls->IsActive()) {
        return m_tls->Send(data, size);
    }
    return m_socket.SendAll(data, size) == SocketResult::Ok;
}

bool NetworkConnection::ReadSome(uint8_t* data, size_t capacity, size_t& outReceived) {
    if (!IsConnected()) {
        return false;
    }

    if (m_tls && m_tls->IsActive()) {
        return m_tls->Recv(data, capacity, outReceived);
    }

    const SocketResult result = m_socket.RecvSome(data, capacity, outReceived);
    if (result == SocketResult::Disconnected || result == SocketResult::Error) {
        Close();
        return false;
    }
    if (result == SocketResult::WouldBlock) {
        outReceived = 0;
    }
    return true;
}

bool NetworkConnection::ReadExact(uint8_t* data, size_t size) {
    if (!IsConnected()) {
        return false;
    }

    if (m_tls && m_tls->IsActive()) {
        return m_tls->RecvExact(data, size);
    }
    return m_socket.RecvAll(data, size) == SocketResult::Ok;
}

bool NetworkConnection::SendPacket(const Engine::Net::NetPacket& message) {
    if (!IsConnected()) {
        return false;
    }

    const Engine::Net::NetBuffer wire = Engine::Net::SerializePacket(message);
    return WriteExact(wire.Data().data(), wire.Data().size());
}

bool NetworkConnection::ReceivePacket(Engine::Net::NetPacket& outMessage) {
    if (!IsConnected()) {
        return false;
    }

    while (m_readBuffer.size() < Engine::Net::kMessageHeaderSize) {
        uint8_t chunk[4096];
        size_t received = 0;
        if (!ReadSome(chunk, sizeof(chunk), received)) {
            return false;
        }
        if (received == 0) {
            return false;
        }
        m_readBuffer.insert(m_readBuffer.end(), chunk, chunk + received);
    }

    Engine::Net::MessageHeader header{};
    Engine::Net::NetBuffer headerBuffer(
        std::vector<uint8_t>(m_readBuffer.begin(), m_readBuffer.begin() + Engine::Net::kMessageHeaderSize));
    if (!headerBuffer.ReadU32(header.magic) ||
        !headerBuffer.ReadU16(header.version) ||
        !headerBuffer.ReadU16(header.type) ||
        !headerBuffer.ReadU32(header.payloadSize) ||
        !headerBuffer.ReadU64(header.sequence)) {
        Close();
        return false;
    }

    if (header.payloadSize > Engine::Net::kMaxPayloadBytes) {
        LOG_WARNING_FMT("NetworkConnection",
                        "Rejected oversized payload (%u > %u)",
                        header.payloadSize,
                        Engine::Net::kMaxPayloadBytes);
        Close();
        return false;
    }

    const size_t totalSize = Engine::Net::kMessageHeaderSize + header.payloadSize;
    while (m_readBuffer.size() < totalSize) {
        uint8_t chunk[4096];
        size_t received = 0;
        if (!ReadSome(chunk, sizeof(chunk), received)) {
            Close();
            return false;
        }
        if (received == 0) {
            return false;
        }
        m_readBuffer.insert(m_readBuffer.end(), chunk, chunk + received);
    }

    if (!Engine::Net::DeserializePacket(m_readBuffer.data(), totalSize, outMessage)) {
        Close();
        return false;
    }

    m_readBuffer.erase(m_readBuffer.begin(), m_readBuffer.begin() + static_cast<std::ptrdiff_t>(totalSize));
    return true;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
