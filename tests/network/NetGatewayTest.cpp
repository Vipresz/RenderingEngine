#include <gtest/gtest.h>

#include "TestPort.h"
#include "network/NetGateway.h"
#include "network/NetworkConnection.h"
#include "network/Socket.h"

#include "core/network/NetworkPayloads.h"

#include <chrono>
#include <thread>

TEST(NetGatewayTest, RedirectsLoginToRegisteredShard) {
    Engine::Platform::Networking::NetGateway gateway;
    const uint16_t port = Engine::Tests::AllocateEphemeralTestPort(61);
    ASSERT_TRUE(gateway.Start("127.0.0.1", port));

    Engine::Net::ShardEndpoint shard;
    shard.shardId = 7;
    shard.host = "127.0.0.1";
    shard.tcpPort = 7777;
    shard.udpPort = 7778;
    shard.maxPlayers = 256;
    gateway.GetRegistry().RegisterShard(shard);
    gateway.GetRegistry().UpdatePopulation(
        7,
        12,
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count()));

    Engine::Platform::Networking::Socket clientSocket = Engine::Platform::Networking::Socket::CreateTcp();
    ASSERT_TRUE(clientSocket.Connect("127.0.0.1", port));
    Engine::Platform::Networking::NetworkConnection connection(std::move(clientSocket));

    Engine::Net::LoginRequestPayload request;
    request.displayName = "gateway_player";
    ASSERT_TRUE(connection.SendPacket(Engine::Net::MakeMessage(
        Engine::Net::MessageType::LoginRequest, request.Serialize())));

    Engine::Net::NetPacket responsePacket;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (connection.ReceivePacket(responsePacket)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    Engine::Net::LoginResponsePayload response;
    ASSERT_EQ(responsePacket.GetType(), Engine::Net::MessageType::LoginResponse);
    ASSERT_TRUE(Engine::Net::LoginResponsePayload::Deserialize(responsePacket.payload, response));
    EXPECT_TRUE(response.redirect);
    EXPECT_EQ(response.shardId, 7u);
    EXPECT_EQ(response.redirectTcpPort, 7777u);

    gateway.Stop();
}
