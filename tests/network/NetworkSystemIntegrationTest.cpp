#include <gtest/gtest.h>

#include "network/NetServer.h"
#include "network/systems/NetworkSystem.h"
#include "TestPort.h"

#include "core/ecs/Registry.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using Engine::ECS::Registry;
using Engine::Platform::Networking::NetServer;
using Engine::Platform::Networking::Systems::NetworkSystem;

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

void PumpNetwork(NetworkSystem& networkSystem, int iterations = 1) {
    for (int i = 0; i < iterations; ++i) {
        networkSystem.PollNetwork();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // namespace

class NetworkSystemIntegrationEnvironment : public ::testing::Test {
protected:
    NetServer server;
    NetworkSystem networkSystem;
    Registry registry;
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;

    void SetUp() override {
        tcpPort = Engine::Tests::AllocateEphemeralTestPort(70);
        udpPort = static_cast<uint16_t>(tcpPort + 1);
        ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));
    }

    void TearDown() override {
        networkSystem.Disconnect();
        server.Stop();
    }
};

TEST_F(NetworkSystemIntegrationEnvironment, ConnectLoginAndMeasurePing) {
    ASSERT_TRUE(networkSystem.ConnectAndLogin(kHost, tcpPort, "network_system_itest"));

    const bool loggedIn = WaitUntil(
        [&]() {
            PumpNetwork(networkSystem, 1);
            return networkSystem.IsLoggedIn();
        },
        std::chrono::seconds(3));
    ASSERT_TRUE(loggedIn);
    EXPECT_NE(networkSystem.GetLocalUserId(), Engine::Net::kInvalidUserId);

    networkSystem.GetClient().SendPing();
    const bool measured = WaitUntil(
        [&]() {
            PumpNetwork(networkSystem, 1);
            return networkSystem.GetRoundTripTimeMs() > 0.0f;
        },
        std::chrono::seconds(3));

    EXPECT_TRUE(measured);
    EXPECT_LT(networkSystem.GetRoundTripTimeMs(), 500.0f);
}

TEST_F(NetworkSystemIntegrationEnvironment, GenericDataHandlerReceivesBroadcast) {
    std::atomic<bool> received{false};
    networkSystem.SetGenericDataHandler([&](const Engine::Net::GenericDataPayload& payload) {
        if (payload.channel == "test_channel") {
            received.store(true);
        }
    });

    ASSERT_TRUE(networkSystem.ConnectAndLogin(kHost, tcpPort, "generic_data_itest"));
    ASSERT_TRUE(WaitUntil(
        [&]() {
            PumpNetwork(networkSystem, 1);
            return networkSystem.IsLoggedIn();
        },
        std::chrono::seconds(3)));

    Engine::Net::GenericDataPayload payload;
    payload.channel = "test_channel";
    payload.data = {1, 2, 3};
    server.BroadcastGenericData(payload);

    EXPECT_TRUE(WaitUntil(
        [&]() {
            PumpNetwork(networkSystem, 1);
            return received.load();
        },
        std::chrono::seconds(3)));
}
