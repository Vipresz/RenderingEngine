#include <gtest/gtest.h>

#include "network/NetClient.h"
#include "network/NetServer.h"
#include "TestPort.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;

namespace {

constexpr const char* kHost = "127.0.0.1";

bool WaitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

} // namespace

class PingPongTest : public ::testing::Test {
protected:
    NetServer server;
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;

    void SetUp() override {
        tcpPort = Engine::Tests::AllocateEphemeralTestPort(40);
        udpPort = static_cast<uint16_t>(tcpPort + 1);
        ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));
    }

    void TearDown() override {
        server.Stop();
    }
};

TEST_F(PingPongTest, PingReturnsRoundTripTime) {
    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, tcpPort));
    ASSERT_TRUE(client.Login("ping_player"));

    ASSERT_TRUE(client.SendPing());
    const bool measured = WaitUntil(
        [&]() {
            client.Poll();
            return client.GetRoundTripTimeMs() > 0.0f;
        },
        std::chrono::seconds(2));

    EXPECT_TRUE(measured);
    EXPECT_LT(client.GetRoundTripTimeMs(), 500.0f);
    client.Disconnect();
}
