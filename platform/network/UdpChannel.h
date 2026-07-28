#pragma once

#include "core/network/NetPacket.h"
#include "core/network/NetTypes.h"
#include "core/network/NetworkPayloads.h"
#include "network/DatagramSocket.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Engine {
namespace Platform {
namespace Networking {

struct UdpDatagramContext {
    std::string host;
    uint16_t port = 0;
};

using UnreliableTransformHandler =
    std::function<void(const Engine::Net::TransformStatePayload&, const UdpDatagramContext&)>;
using UdpSessionRegisterHandler =
    std::function<void(const Engine::Net::UdpSessionRegisterPayload&, const UdpDatagramContext&)>;

// Lightweight unreliable channel for high-frequency movement updates over UDP.
class UdpChannel {
public:
    bool Bind(const std::string& host, uint16_t port);
    bool Connect(const std::string& host, uint16_t port);
    void Close();

    bool IsOpen() const { return m_socket.IsValid(); }

    void SetUnreliableTransformHandler(UnreliableTransformHandler handler) {
        m_transformHandler = std::move(handler);
    }
    void SetUdpSessionRegisterHandler(UdpSessionRegisterHandler handler) {
        m_registerHandler = std::move(handler);
    }

    bool SendUnreliableTransform(const Engine::Net::TransformStatePayload& payload);
    bool SendUdpSessionRegister(const Engine::Net::UdpSessionRegisterPayload& payload);
    void Poll();

private:
    struct UdpPacketHeader {
        uint32_t magic = Engine::Net::kUdpChannelMagic;
        uint16_t version = Engine::Net::kProtocolVersion;
        uint16_t type = static_cast<uint16_t>(Engine::Net::MessageType::UnreliableTransform);
        uint64_t sequence = 0;
    };

    bool SendPayload(Engine::Net::MessageType type, const Engine::Net::NetBuffer& payload);
    void HandleDatagram(const uint8_t* data, size_t size, const std::string& host, uint16_t port);

    DatagramSocket m_socket;
    UnreliableTransformHandler m_transformHandler;
    UdpSessionRegisterHandler m_registerHandler;
    uint64_t m_sendSequence = 1;
    std::string m_remoteHost;
    uint16_t m_remotePort = 0;
    bool m_connected = false;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
