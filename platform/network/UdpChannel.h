#pragma once

#include "core/network/NetPacket.h"
#include "core/network/MovementCommand.h"
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
using UdpMovementCommandHandler =
    std::function<void(const Engine::Net::MovementCommand&, const UdpDatagramContext&)>;
using UdpMovementAckHandler =
    std::function<void(const Engine::Net::MovementAckPayload&, const UdpDatagramContext&)>;

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
    void SetUdpMovementCommandHandler(UdpMovementCommandHandler handler) {
        m_movementHandler = std::move(handler);
    }
    void SetUdpMovementAckHandler(UdpMovementAckHandler handler) {
        m_movementAckHandler = std::move(handler);
    }

    void SetSessionToken(uint64_t sessionToken) { m_sessionToken = sessionToken; }
    uint64_t GetSessionToken() const { return m_sessionToken; }

    using SessionTokenResolver = std::function<uint64_t(const UdpDatagramContext&)>;
    void SetSessionTokenResolver(SessionTokenResolver resolver) {
        m_sessionTokenResolver = std::move(resolver);
    }

    void SetRequireAuthenticatedDatagrams(bool required) { m_requireAuthenticatedDatagrams = required; }

    bool SendUnreliableTransform(const Engine::Net::TransformStatePayload& payload);
    bool SendMovementCommand(const Engine::Net::MovementCommand& command);
    bool SendUdpSessionRegister(const Engine::Net::UdpSessionRegisterPayload& payload);
    bool SendToEndpoint(const std::string& host,
                        uint16_t port,
                        Engine::Net::MessageType type,
                        const Engine::Net::NetBuffer& payload,
                        uint64_t sessionTokenForAuth);
    void Poll();
    size_t PollAndDrain();

private:
    struct UdpPacketHeader {
        uint32_t magic = Engine::Net::kUdpChannelMagic;
        uint16_t version = Engine::Net::kProtocolVersion;
        uint16_t type = static_cast<uint16_t>(Engine::Net::MessageType::UnreliableTransform);
        uint64_t sequence = 0;
        uint64_t authTag = 0;
    };

    bool SendPayload(Engine::Net::MessageType type, const Engine::Net::NetBuffer& payload);
    bool SendPayloadTo(Engine::Net::MessageType type,
                       const Engine::Net::NetBuffer& payload,
                       const std::string& host,
                       uint16_t port,
                       uint64_t sessionTokenForAuth);
    void HandleDatagram(const uint8_t* data, size_t size, const std::string& host, uint16_t port);

    DatagramSocket m_socket;
    UnreliableTransformHandler m_transformHandler;
    UdpSessionRegisterHandler m_registerHandler;
    UdpMovementCommandHandler m_movementHandler;
    UdpMovementAckHandler m_movementAckHandler;
    uint64_t m_sendSequence = 1;
    uint64_t m_sessionToken = 0;
    SessionTokenResolver m_sessionTokenResolver;
    bool m_requireAuthenticatedDatagrams = true;
    std::string m_remoteHost;
    uint16_t m_remotePort = 0;
    bool m_connected = false;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
