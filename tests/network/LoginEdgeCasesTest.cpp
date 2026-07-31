#include <gtest/gtest.h>

#include "network/NetClient.h"
#include "network/NetServer.h"
#include "TestPort.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using Engine::Platform::Networking::ClientCallbacks;
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

class LoginEdgeCasesTest : public ::testing::Test {
protected:
    NetServer server;
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;

    void SetUp() override {
        tcpPort = Engine::Tests::AllocateEphemeralTestPort(50);
        udpPort = static_cast<uint16_t>(tcpPort + 1);
        ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));
    }

    void TearDown() override {
        server.Stop();
    }
};

TEST_F(LoginEdgeCasesTest, PreferredUserIdMigratesConnectionSlot) {
    NetClient firstClient;
    ASSERT_TRUE(firstClient.Connect(kHost, tcpPort));
    ASSERT_TRUE(firstClient.Login("slot_owner"));
    const Engine::Net::UserId preferredId = firstClient.GetUserId();
    firstClient.Disconnect();

    NetClient secondClient;
    ASSERT_TRUE(secondClient.Connect(kHost, tcpPort));
    ASSERT_NE(secondClient.GetUserId(), preferredId);
    ASSERT_TRUE(secondClient.Login("slot_owner", preferredId));

    EXPECT_EQ(secondClient.GetUserId(), preferredId);
    secondClient.Disconnect();
}

TEST_F(LoginEdgeCasesTest, ReLoginDoesNotInvokeLoginCallbackAgain) {
    std::atomic<int> loginCallbacks{0};
    server.SetClientLoggedInCallback([&](Engine::Net::UserId, const std::string&) {
        loginCallbacks.fetch_add(1);
    });

    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, tcpPort));
    ASSERT_TRUE(client.Login("relogin_player"));
    EXPECT_EQ(loginCallbacks.load(), 1);

    ASSERT_TRUE(client.Login("relogin_player"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(loginCallbacks.load(), 1);

    client.Disconnect();
}
