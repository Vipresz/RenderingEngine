#pragma once

#include "core/network/NetPacket.h"
#include "core/network/NetworkPayloads.h"
#include "network/MessageQueue.h"
#include "network/NetworkConnection.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace Engine {
namespace Platform {
namespace Networking {

using NetPacket = ::Engine::Net::NetPacket;
using NetUserId = ::Engine::Net::UserId;

struct ClientCallbacks {
    std::function<void(NetUserId, const std::string&)> onPeerJoined;
    std::function<void(NetUserId)> onPeerLeft;
    std::function<void(const ::Engine::Net::TransformStatePayload&)> onTransformState;
    std::function<void(const ::Engine::Net::GenericDataPayload&)> onGenericData;
};

class NetClient {
public:
    NetClient();
    ~NetClient();

    bool Connect(const std::string& host, uint16_t port);
    bool Login(const std::string& displayName, NetUserId preferredUserId = ::Engine::Net::kInvalidUserId);
    void Disconnect();

    bool IsConnected() const { return m_connected; }
    bool IsLoggedIn() const { return m_loggedIn; }
    NetUserId GetUserId() const { return m_userId; }

    void SetCallbacks(ClientCallbacks callbacks) { m_callbacks = std::move(callbacks); }

    bool SendTransformState(const Engine::Net::TransformStatePayload& payload);
    bool SendGenericData(const Engine::Net::GenericDataPayload& payload);

    void Poll();

private:
    void ReceiveLoop();
    void HandlePacket(const NetPacket& message);
    bool SendPacket(const NetPacket& message);

    std::unique_ptr<NetworkConnection> m_connection;
    std::thread m_receiveThread;
    MessageQueue<NetPacket> m_incoming;
    ClientCallbacks m_callbacks;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_loggedIn{false};
    NetUserId m_userId = ::Engine::Net::kInvalidUserId;
    uint64_t m_sequence = 1;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
