#include <gtest/gtest.h>

#include "network/UdpChannel.h"

#include <atomic>
#include <chrono>
#include <thread>

using Engine::Net::TransformStatePayload;
using Engine::Platform::Networking::UdpChannel;

TEST(UdpChannelTest, LoopbackUnreliableTransform) {
    UdpChannel server;
    UdpChannel client;

    ASSERT_TRUE(server.Bind("127.0.0.1", 19000));
    ASSERT_TRUE(client.Connect("127.0.0.1", 19000));

    std::atomic<bool> received{false};
    TransformStatePayload receivedPayload;

    server.SetUnreliableTransformHandler([&](const TransformStatePayload& payload) {
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
