#include <gtest/gtest.h>

#include "network/NetClient.h"
#include "network/NetServer.h"
#include "TestPort.h"

#include <chrono>
#include <thread>

using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;

namespace {

constexpr const char* kHost = "127.0.0.1";

} // namespace

class NetServerSecurityTest : public ::testing::Test {
protected:
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;
};

TEST_F(NetServerSecurityTest, RefusesToStartWithoutAuthenticationConfigured) {
    NetServer server;
    Engine::Net::ServerSecurityPolicy policy;
    policy.enforceAuthentication = true;
    server.SetSecurityPolicy(policy);

    tcpPort = Engine::Tests::AllocateEphemeralTestPort(60);
    udpPort = static_cast<uint16_t>(tcpPort + 1);
    EXPECT_FALSE(server.Start(kHost, tcpPort, udpPort));
}

TEST_F(NetServerSecurityTest, RejectsOpenLoginWhenDisabled) {
    NetServer server;
    Engine::Net::ServerSecurityPolicy policy;
    policy.allowOpenLogin = false;
    server.SetSecurityPolicy(policy);

    tcpPort = Engine::Tests::AllocateEphemeralTestPort(61);
    udpPort = static_cast<uint16_t>(tcpPort + 1);
    ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));

    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, tcpPort));
    EXPECT_FALSE(client.Login("open_login_player"));
    client.Disconnect();
    server.Stop();
}

TEST_F(NetServerSecurityTest, EncryptedUdpMovementRoundTrip) {
    NetServer server;
    Engine::Net::ServerSecurityPolicy policy;
    policy.enforceAuthentication = true;
    policy.encryptUdpPayloads = true;
    policy.allowOpenLogin = false;
    server.SetSecurityPolicy(policy);
    server.SetRequiredAuthToken("security-test-token");

    tcpPort = Engine::Tests::AllocateEphemeralTestPort(62);
    udpPort = static_cast<uint16_t>(tcpPort + 1);
    ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));

    NetClient client;
    client.SetAuthToken("security-test-token");
    ASSERT_TRUE(client.Connect(kHost, tcpPort));
    ASSERT_TRUE(client.Login("secure_player"));
    ASSERT_TRUE(client.ConnectUdp(kHost, udpPort));
    ASSERT_TRUE(client.RegisterUdpSession());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && !server.HasUdpEndpointFor(client.GetUserId())) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(server.HasUdpEndpointFor(client.GetUserId()));

    Engine::Net::TransformStatePayload sent;
    sent.userId = client.GetUserId();
    sent.tick = 1;
    sent.position = glm::vec3(8.0f, 2.0f, -3.0f);
    ASSERT_TRUE(client.SendUnreliableTransform(sent));

    bool authorityUpdated = false;
    const auto verifyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < verifyDeadline) {
        const Engine::Net::TransformStatePayload* authoritative =
            server.GetServerAuthority().GetAuthoritative(client.GetUserId());
        if (authoritative && glm::length(authoritative->position - sent.position) < 0.01f) {
            authorityUpdated = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(authorityUpdated);

    client.Disconnect();
    server.Stop();
}

TEST(NetClientSecurityTest, ClearsStoredPasswordAfterLogin) {
    NetServer server;
    server.GetCredentialStore().RegisterUser("alice", "secret123");

    const uint16_t tcpPort = Engine::Tests::AllocateEphemeralTestPort(63);
    const uint16_t udpPort = static_cast<uint16_t>(tcpPort + 1);
    ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));

    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, tcpPort));
    client.SetCredentials("alice", "secret123");
    ASSERT_TRUE(client.Login("alice_display"));
    client.Disconnect();

    ASSERT_TRUE(client.Connect(kHost, tcpPort));
    EXPECT_FALSE(client.Login("alice_display"));

    client.Disconnect();
    server.Stop();
}
