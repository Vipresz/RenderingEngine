#include <gtest/gtest.h>

#include "network/UdpChannel.h"
#include "TestPort.h"

#include <atomic>
#include <chrono>
#include <thread>

using Engine::Net::TransformStatePayload;
using Engine::Net::UdpSessionRegisterPayload;
using Engine::Platform::Networking::UdpChannel;
using Engine::Platform::Networking::UdpDatagramContext;

TEST(UdpChannelTest, LoopbackUnreliableTransform) {
    UdpChannel server;
    UdpChannel client;

    const uint16_t port = Engine::Tests::AllocateEphemeralTestPort(0);
    ASSERT_TRUE(server.Bind("127.0.0.1", port));
    ASSERT_TRUE(client.Connect("127.0.0.1", port));

    std::atomic<bool> received{false};
    TransformStatePayload receivedPayload;

    server.SetUnreliableTransformHandler([&](const TransformStatePayload& payload, const UdpDatagramContext&) {
        receivedPayload = payload;
        received = true;
    });

    TransformStatePayload sent;
    sent.userId = 7;
    sent.tick = 99;
    sent.position = glm::vec3(1.0f, 2.0f, 3.0f);
    ASSERT_TRUE(client.SendUnreliableTransform(sent));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!received && std::chrono::steady_clock::now() < deadline) {
        server.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(received.load());
    EXPECT_EQ(receivedPayload.userId, 7u);
    EXPECT_EQ(receivedPayload.tick, 99u);
    EXPECT_FLOAT_EQ(receivedPayload.position.z, 3.0f);
}

TEST(UdpChannelTest, LoopbackSessionRegister) {
    UdpChannel server;
    UdpChannel client;

    const uint16_t port = Engine::Tests::AllocateEphemeralTestPort(1);
    ASSERT_TRUE(server.Bind("127.0.0.1", port));
    ASSERT_TRUE(client.Connect("127.0.0.1", port));

    std::atomic<bool> received{false};
    UdpSessionRegisterPayload receivedPayload;

    server.SetUdpSessionRegisterHandler([&](const UdpSessionRegisterPayload& payload, const UdpDatagramContext& ctx) {
        receivedPayload = payload;
        EXPECT_FALSE(ctx.host.empty());
        EXPECT_GT(ctx.port, 0u);
        received = true;
    });

    UdpSessionRegisterPayload sent;
    sent.userId = 3;
    sent.sessionToken = 0xABCDEF01ull;
    ASSERT_TRUE(client.SendUdpSessionRegister(sent));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!received && std::chrono::steady_clock::now() < deadline) {
        server.Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(received.load());
    EXPECT_EQ(receivedPayload.userId, 3u);
    EXPECT_EQ(receivedPayload.sessionToken, 0xABCDEF01ull);
}
