#pragma once

#include "core/network/NetPacket.h"
#include "network/Socket.h"
#include "network/TlsSession.h"

#include <memory>
#include <vector>

namespace Engine {
namespace Platform {
namespace Networking {

using NetPacket = ::Engine::Net::NetPacket;

class NetworkConnection {
public:
    explicit NetworkConnection(Socket socket);

    bool IsConnected() const { return m_socket.IsValid(); }
    Socket& GetSocket() { return m_socket; }

    bool EnableTlsClient(const TlsConfig& config);
    bool EnableTlsServer(const TlsConfig& config);

    bool SendPacket(const NetPacket& message);
    bool ReceivePacket(NetPacket& outMessage);

    void Close();

private:
    bool ReadExact(uint8_t* data, size_t size);
    bool WriteExact(const uint8_t* data, size_t size);
    bool ReadSome(uint8_t* data, size_t capacity, size_t& outReceived);

    Socket m_socket;
    std::unique_ptr<TlsSession> m_tls;
    std::vector<uint8_t> m_readBuffer;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
