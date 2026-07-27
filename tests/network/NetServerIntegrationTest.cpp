#include <gtest/gtest.h>

#include "network/NetClient.h"
#include "network/NetServer.h"
#include "network/UdpChannel.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using Engine::Net::TransformStatePayload;
using Engine::Net::UdpSessionRegisterPayload;
using Engine::Platform::Networking::ClientCallbacks;
using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;
using Engine::Platform::Networking::UdpChannel;

namespace {

constexpr const char* kHost = "127.0.0.1";
constexpr uint16_t kTcpPort = 19210;
constexpr uint16_t kUdpPort = 19211;

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

class NetServerIntegrationTest : public ::testing::Test {
protected:
    NetServer server;

    void SetUp() override {
        ASSERT_TRUE(server.Start(kHost, kTcpPort, kUdpPort));
    }

    void TearDown() override {
        server.Stop();
    }
};

TEST_F(NetServerIntegrationTest, LoginRegisterUdpAndMoveUpdatesAuthority) {
    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, kTcpPort));
    ASSERT_TRUE(client.Login("integration_player"));
    ASSERT_TRUE(client.ConnectUdp(kHost, kUdpPort));
    ASSERT_TRUE(client.RegisterUdpSession());

    TransformStatePayload sent;
    sent.userId = client.GetUserId();
    sent.tick = 1;
    sent.position = glm::vec3(3.0f, 1.0f, 4.0f);
    sent.rotationDegrees = glm::vec3(0.0f, 45.0f, 0.0f);
    ASSERT_TRUE(client.SendUnreliableTransform(sent));

    const bool authorityUpdated = WaitUntil(
        [&]() {
            const TransformStatePayload* authoritative =
                server.GetServerAuthority().GetAuthoritative(client.GetUserId());
            return authoritative &&
                   glm::length(authoritative->position - sent.position) < 0.01f;
        },
        std::chrono::seconds(3));

    ASSERT_TRUE(authorityUpdated);
    const TransformStatePayload* authoritative =
        server.GetServerAuthority().GetAuthoritative(client.GetUserId());
    ASSERT_NE(authoritative, nullptr);
    EXPECT_FLOAT_EQ(authoritative->position.x, sent.position.x);
    EXPECT_FLOAT_EQ(authoritative->position.y, sent.position.y);
    EXPECT_FLOAT_EQ(authoritative->position.z, sent.position.z);

    client.Disconnect();
}

TEST_F(NetServerIntegrationTest, UnregisteredUdpMovementIsIgnored) {
    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, kTcpPort));
    ASSERT_TRUE(client.Login("unregistered_udp"));
    ASSERT_TRUE(client.ConnectUdp(kHost, kUdpPort));

    TransformStatePayload sent;
    sent.userId = client.GetUserId();
    sent.tick = 1;
    sent.position = glm::vec3(9.0f, 0.0f, 9.0f);
    ASSERT_TRUE(client.SendUnreliableTransform(sent));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(server.GetServerAuthority().GetAuthoritative(client.GetUserId()), nullptr);

    client.Disconnect();
}

TEST_F(NetServerIntegrationTest, BadUdpSessionTokenDoesNotBindEndpoint) {
    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, kTcpPort));
    ASSERT_TRUE(client.Login("bad_token_player"));
    ASSERT_TRUE(client.ConnectUdp(kHost, kUdpPort));

    UdpChannel rogueChannel;
    ASSERT_TRUE(rogueChannel.Connect(kHost, kUdpPort));

    UdpSessionRegisterPayload badRegistration;
    badRegistration.userId = client.GetUserId();
    badRegistration.sessionToken = 0xDEADBEEFull;
    ASSERT_TRUE(rogueChannel.SendUdpSessionRegister(badRegistration));

    TransformStatePayload sent;
    sent.userId = client.GetUserId();
    sent.tick = 1;
    sent.position = glm::vec3(2.0f, 0.0f, 2.0f);
    ASSERT_TRUE(client.SendUnreliableTransform(sent));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(server.GetServerAuthority().GetAuthoritative(client.GetUserId()), nullptr);

    client.Disconnect();
}

TEST_F(NetServerIntegrationTest, LoginRegisterUdpAndMoveEchoesAuthoritativeState) {
    NetClient client;
    std::atomic<bool> receivedEcho{false};

    ClientCallbacks callbacks;
    callbacks.onTransformState = [&](const TransformStatePayload& transform) {
        if (transform.userId == client.GetUserId() &&
            glm::length(transform.position - glm::vec3(3.0f, 1.0f, 4.0f)) < 0.01f) {
            receivedEcho = true;
        }
    };
    client.SetCallbacks(std::move(callbacks));

    ASSERT_TRUE(client.Connect(kHost, kTcpPort));
    ASSERT_TRUE(client.Login("echo_player"));
    ASSERT_TRUE(client.ConnectUdp(kHost, kUdpPort));
    ASSERT_TRUE(client.RegisterUdpSession());

    TransformStatePayload sent;
    sent.userId = client.GetUserId();
    sent.tick = 1;
    sent.position = glm::vec3(3.0f, 1.0f, 4.0f);
    sent.rotationDegrees = glm::vec3(0.0f, 45.0f, 0.0f);
    ASSERT_TRUE(client.SendUnreliableTransform(sent));

    const bool echoed = WaitUntil(
        [&]() {
            client.Poll();
            return receivedEcho.load();
        },
        std::chrono::seconds(3));
    EXPECT_TRUE(echoed);

    client.Disconnect();
}

class NetServerAuthTest : public ::testing::Test {
protected:
    NetServer server;

    void SetUp() override {
        server.SetRequiredAuthToken("integration-secret");
        ASSERT_TRUE(server.Start(kHost, kTcpPort + 10, kUdpPort + 10));
    }

    void TearDown() override {
        server.Stop();
    }
};

class NetServerCredentialTest : public ::testing::Test {
protected:
    NetServer server;

    void SetUp() override {
        server.GetCredentialStore().SetRequireCredentials(true);
        ASSERT_TRUE(server.GetCredentialStore().RegisterUser("alice", "secret123"));
        ASSERT_TRUE(server.Start(kHost, kTcpPort + 20, kUdpPort + 20));
    }

    void TearDown() override {
        server.Stop();
    }
};

TEST_F(NetServerAuthTest, RejectsLoginWithMissingOrWrongAuthToken) {
    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, kTcpPort + 10));
    EXPECT_FALSE(client.Login("no_token_player"));

    client.Disconnect();
    ASSERT_TRUE(client.Connect(kHost, kTcpPort + 10));
    client.SetAuthToken("wrong-secret");
    EXPECT_FALSE(client.Login("bad_token_player"));

    client.Disconnect();
    ASSERT_TRUE(client.Connect(kHost, kTcpPort + 10));
    client.SetAuthToken("integration-secret");
    EXPECT_TRUE(client.Login("good_token_player"));

    client.Disconnect();
}

TEST_F(NetServerCredentialTest, RejectsInvalidCredentialsAndAcceptsValidLogin) {
    NetClient client;
    ASSERT_TRUE(client.Connect(kHost, kTcpPort + 20));
    client.SetCredentials("alice", "wrong");
    EXPECT_FALSE(client.Login("alice_display"));

    client.Disconnect();
    ASSERT_TRUE(client.Connect(kHost, kTcpPort + 20));
    client.SetCredentials("alice", "secret123");
    EXPECT_TRUE(client.Login("alice_display"));

    client.Disconnect();
}

class NetServerAuthoritativeModeTest : public ::testing::Test {
protected:
    NetServer server;

    void SetUp() override {
        server.SetAuthoritativeMode(true);
        ASSERT_TRUE(server.Start(kHost, kTcpPort + 30, kUdpPort + 30));
    }

    void TearDown() override {
        server.Stop();
    }
};

TEST_F(NetServerAuthoritativeModeTest, DropsClientEntitySnapshotsAndTransforms) {
    NetClient publisher;
    NetClient observer;
    std::atomic<bool> observerSawSnapshot{false};
    std::atomic<bool> observerSawTransform{false};

    ClientCallbacks observerCallbacks;
    observerCallbacks.onEntitySnapshot = [&](const Engine::Net::EntitySnapshot&) {
        observerSawSnapshot = true;
    };
    observerCallbacks.onTransformState = [&](const TransformStatePayload&) {
        observerSawTransform = true;
    };
    observer.SetCallbacks(std::move(observerCallbacks));

    ASSERT_TRUE(publisher.Connect(kHost, kTcpPort + 30));
    ASSERT_TRUE(publisher.Login("auth_publisher"));
    ASSERT_TRUE(publisher.ConnectUdp(kHost, kUdpPort + 30));
    ASSERT_TRUE(publisher.RegisterUdpSession());
    ASSERT_TRUE(observer.Connect(kHost, kTcpPort + 30));
    ASSERT_TRUE(observer.Login("auth_observer"));

    Engine::Net::EntitySnapshot snapshot;
    snapshot.entityId = publisher.GetUserId();
    snapshot.tick = 99;
    ASSERT_TRUE(publisher.SendEntitySnapshot(snapshot));

    TransformStatePayload transform;
    transform.userId = publisher.GetUserId();
    transform.tick = 1;
    transform.position = glm::vec3(8.0f, 1.0f, 8.0f);
    ASSERT_TRUE(publisher.SendUnreliableTransform(transform));

    const bool leaked = WaitUntil(
        [&]() {
            observer.Poll();
            return observerSawSnapshot.load() || observerSawTransform.load();
        },
        std::chrono::milliseconds(500));
    EXPECT_FALSE(leaked);
    EXPECT_EQ(server.GetServerAuthority().GetAuthoritative(publisher.GetUserId()), nullptr);

    publisher.Disconnect();
    observer.Disconnect();
}
