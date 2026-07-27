#pragma once

#include "core/network/NetPacket.h"
#include "core/network/NetworkPayloads.h"
#include "core/network/ReplicationTypes.h"
#include "network/MessageQueue.h"
#include "network/NetworkConnection.h"
#include "network/UdpChannel.h"

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
    std::function<void(const ::Engine::Net::WorldSnapshotPayload&)> onWorldSnapshot;
    std::function<void(const ::Engine::Net::EntitySnapshot&)> onEntitySnapshot;
    std::function<void(const ::Engine::Net::ComponentSnapshot&)> onComponentSnapshot;
};

class NetClient {
public:
    NetClient();
    ~NetClient();

    bool Connect(const std::string& host, uint16_t port);
    bool ConnectUdp(const std::string& host, uint16_t udpPort = ::Engine::Net::kDefaultUdpPort);
    bool Login(const std::string& displayName, NetUserId preferredUserId = ::Engine::Net::kInvalidUserId);
    bool RegisterUdpSession();
    void Disconnect();

    bool IsConnected() const { return m_connected; }
    bool IsLoggedIn() const { return m_loggedIn; }
    NetUserId GetUserId() const { return m_userId; }

    void SetCallbacks(ClientCallbacks callbacks) { m_callbacks = std::move(callbacks); }

    bool SendTransformState(const Engine::Net::TransformStatePayload& payload);
    bool SendUnreliableTransform(const Engine::Net::TransformStatePayload& payload);
    bool SendGenericData(const Engine::Net::GenericDataPayload& payload);
    bool SendEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot);
    bool SendWorldSnapshot(const Engine::Net::WorldSnapshotPayload& snapshot);

    void SetUseUdpForMovement(bool enabled) { m_useUdpForMovement = enabled; }
    bool UsesUdpForMovement() const { return m_useUdpForMovement; }

    void Poll();

private:
    void ReceiveLoop();
    void HandlePacket(const NetPacket& message);
    bool SendPacket(const NetPacket& message);

    std::unique_ptr<NetworkConnection> m_connection;
    UdpChannel m_udpChannel;
    std::thread m_receiveThread;
    MessageQueue<NetPacket> m_incoming;
    ClientCallbacks m_callbacks;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_loggedIn{false};
    NetUserId m_userId = ::Engine::Net::kInvalidUserId;
    uint64_t m_sequence = 1;
    bool m_useUdpForMovement = true;
    std::string m_host;
    uint16_t m_udpPort = ::Engine::Net::kDefaultUdpPort;
    uint64_t m_udpSessionToken = 0;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
