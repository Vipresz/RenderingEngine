#pragma once

#include "core/network/NetPacket.h"
#include "network/Socket.h"

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

    bool SendPacket(const NetPacket& message);
    bool ReceivePacket(NetPacket& outMessage);

    void Close();

private:
    bool ReadExact(uint8_t* data, size_t size);
    bool WriteExact(const uint8_t* data, size_t size);

    Socket m_socket;
    std::vector<uint8_t> m_readBuffer;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
