#include <gtest/gtest.h>

#include "network/Socket.h"
#include "TestPort.h"

#include <array>
#include <atomic>
#include <chrono>
#include <thread>

using Engine::Platform::Networking::Socket;
using Engine::Platform::Networking::SocketResult;

namespace {

class SocketNetworkingEnvironment : public ::testing::Environment {
public:
    void SetUp() override { ASSERT_TRUE(Socket::InitializeNetworking()); }
    void TearDown() override { Socket::ShutdownNetworking(); }
};

const auto* kSocketNetworkingEnv =
    ::testing::AddGlobalTestEnvironment(new SocketNetworkingEnvironment());

const uint16_t kLoopbackPort = Engine::Tests::AllocateEphemeralTestPort(100);

} // namespace

TEST(SocketTest, LoopbackTcpSendRecvAll) {
    Socket server = Socket::CreateTcp();
    ASSERT_TRUE(server.IsValid());
    ASSERT_TRUE(server.SetReuseAddress(true));
    ASSERT_TRUE(server.Bind("127.0.0.1", kLoopbackPort));
    ASSERT_TRUE(server.Listen(1));

    std::atomic<bool> serverDone{false};
    std::array<uint8_t, 8> serverBuffer{};

    std::thread serverThread([&] {
        Socket clientConn;
        ASSERT_TRUE(server.Accept(clientConn));
        EXPECT_EQ(clientConn.RecvAll(serverBuffer.data(), serverBuffer.size()), SocketResult::Ok);
        serverDone = true;
    });

    Socket client = Socket::CreateTcp();
    ASSERT_TRUE(client.IsValid());
    ASSERT_TRUE(client.Connect("127.0.0.1", kLoopbackPort));

    const std::array<uint8_t, 8> payload{1, 2, 3, 4, 5, 6, 7, 8};
    EXPECT_EQ(client.SendAll(payload.data(), payload.size()), SocketResult::Ok);

    serverThread.join();
    ASSERT_TRUE(serverDone.load());
    EXPECT_EQ(serverBuffer, payload);
}

TEST(SocketTest, RecvSomeReturnsPartialData) {
    Socket server = Socket::CreateTcp();
    ASSERT_TRUE(server.SetReuseAddress(true));
    ASSERT_TRUE(server.Bind("127.0.0.1", kLoopbackPort + 1));
    ASSERT_TRUE(server.Listen(1));

    std::thread serverThread([&] {
        Socket clientConn;
        ASSERT_TRUE(server.Accept(clientConn));
        const std::array<uint8_t, 6> payload{10, 20, 30, 40, 50, 60};
        EXPECT_EQ(clientConn.SendAll(payload.data(), payload.size()), SocketResult::Ok);
    });

    Socket client = Socket::CreateTcp();
    ASSERT_TRUE(client.Connect("127.0.0.1", kLoopbackPort + 1));
    serverThread.join();

    std::array<uint8_t, 3> buffer{};
    size_t received = 0;
    EXPECT_EQ(client.RecvSome(buffer.data(), buffer.size(), received), SocketResult::Ok);
    EXPECT_EQ(received, 3u);
    EXPECT_EQ(buffer[0], 10);
    EXPECT_EQ(buffer[2], 30);
}

TEST(SocketTest, NonBlockingRecvReturnsWouldBlockWhenEmpty) {
    Socket server = Socket::CreateTcp();
    ASSERT_TRUE(server.SetReuseAddress(true));
    ASSERT_TRUE(server.Bind("127.0.0.1", kLoopbackPort + 2));
    ASSERT_TRUE(server.Listen(1));

    Socket accepted;
    std::atomic<bool> peerConnected{false};
    std::atomic<bool> releaseServer{false};

    std::thread serverThread([&] {
        ASSERT_TRUE(server.Accept(accepted));
        peerConnected = true;
        while (!releaseServer.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    Socket client = Socket::CreateTcp();
    ASSERT_TRUE(client.Connect("127.0.0.1", kLoopbackPort + 2));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!peerConnected.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(peerConnected.load());
    ASSERT_TRUE(client.SetNonBlocking(true));

    std::array<uint8_t, 4> buffer{};
    size_t received = 0;
    EXPECT_EQ(client.RecvSome(buffer.data(), buffer.size(), received), SocketResult::WouldBlock);
    EXPECT_EQ(received, 0u);

    releaseServer = true;
    serverThread.join();
}

TEST(SocketTest, InvalidSocketOperationsFailSafely) {
    Socket invalid;
    EXPECT_FALSE(invalid.IsValid());
    EXPECT_FALSE(invalid.Bind("127.0.0.1", 1));
    EXPECT_FALSE(invalid.Listen());
    EXPECT_FALSE(invalid.Connect("127.0.0.1", 1));
    EXPECT_FALSE(invalid.SetNonBlocking(true));
    EXPECT_FALSE(invalid.SetReuseAddress(true));
    EXPECT_FALSE(invalid.SetTcpNoDelay(true));

    uint8_t byte = 0;
    size_t received = 0;
    EXPECT_EQ(invalid.SendAll(&byte, 1), SocketResult::Error);
    EXPECT_EQ(invalid.RecvAll(&byte, 1), SocketResult::Error);
    EXPECT_EQ(invalid.RecvSome(&byte, 1, received), SocketResult::Error);

    Socket peer;
    EXPECT_FALSE(invalid.Accept(peer));
}

TEST(SocketTest, MoveTransfersOwnership) {
    Socket original = Socket::CreateTcp();
    ASSERT_TRUE(original.IsValid());
    const uintptr_t handle = original.NativeHandle();

    Socket moved = std::move(original);
    EXPECT_FALSE(original.IsValid());
    EXPECT_TRUE(moved.IsValid());
    EXPECT_EQ(moved.NativeHandle(), handle);

    Socket assigned;
    assigned = std::move(moved);
    EXPECT_FALSE(moved.IsValid());
    EXPECT_TRUE(assigned.IsValid());
    EXPECT_EQ(assigned.NativeHandle(), handle);
}
