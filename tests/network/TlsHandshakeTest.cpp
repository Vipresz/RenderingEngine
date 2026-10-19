#include <gtest/gtest.h>

#include "network/Socket.h"
#include "network/TlsSession.h"
#include "TestPort.h"
#include "TestRuntime.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <string>
#include <thread>

#if defined(ENGINE_ENABLE_TLS)

using Engine::Platform::Networking::Socket;
using Engine::Platform::Networking::TlsConfig;
using Engine::Platform::Networking::TlsSession;

namespace {

constexpr const char* kHost = "127.0.0.1";
constexpr const char* kServerName = "localhost";

std::filesystem::path FixtureTlsPath(const char* fileName) {
    const std::filesystem::path runtimePath =
        Engine::Tests::GetExecutableDirectory() / "fixtures" / "tls" / fileName;
    if (std::filesystem::exists(runtimePath)) {
        return runtimePath;
    }
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures" / "tls" / fileName;
}

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

TEST(TlsHandshakeTest, ServerClientExchangePayloadOverTls) {
    ASSERT_TRUE(Engine::Tests::SetWorkingDirectoryToExecutable());

    const std::filesystem::path certPath = FixtureTlsPath("test_server.crt");
    const std::filesystem::path keyPath = FixtureTlsPath("test_server.key");
    ASSERT_TRUE(std::filesystem::exists(certPath)) << certPath.string();
    ASSERT_TRUE(std::filesystem::exists(keyPath)) << keyPath.string();

    ASSERT_TRUE(Socket::InitializeNetworking());

    const uint16_t port = Engine::Tests::AllocateEphemeralTestPort(80);
    Socket listenSocket = Socket::CreateTcp();
    ASSERT_TRUE(listenSocket.IsValid());
    ASSERT_TRUE(listenSocket.SetReuseAddress(true));
    ASSERT_TRUE(listenSocket.Bind(kHost, port));
    ASSERT_TRUE(listenSocket.Listen(4));

    std::atomic<bool> serverDone{false};
    std::atomic<bool> serverOk{false};
    std::string serverError;
    std::promise<void> serverInitialized;
    std::future<void> serverInitializedFuture = serverInitialized.get_future();

    std::thread serverThread([&]() {
        Socket acceptedClient;
        if (!listenSocket.Accept(acceptedClient) || !acceptedClient.IsValid()) {
            serverError = "accept failed";
            serverInitialized.set_value();
            serverDone.store(true);
            return;
        }

        TlsSession serverSession;
        TlsConfig serverConfig;
        serverConfig.enabled = true;
        serverConfig.certificatePath = certPath.string();
        serverConfig.privateKeyPath = keyPath.string();
        if (!serverSession.InitializeServer(acceptedClient.NativeHandle(), serverConfig)) {
            serverError = "server TLS init failed";
            serverInitialized.set_value();
            serverDone.store(true);
            return;
        }
        serverInitialized.set_value();

        if (!serverSession.Handshake()) {
            serverError = "server handshake failed";
            serverDone.store(true);
            return;
        }

        uint8_t request[4] = {};
        if (!serverSession.RecvExact(request, sizeof(request))) {
            serverError = "server recv failed";
            serverDone.store(true);
            return;
        }

        const uint8_t response[4] = {request[0], request[1], request[2], static_cast<uint8_t>(request[3] + 1)};
        if (!serverSession.Send(response, sizeof(response))) {
            serverError = "server send failed";
            serverDone.store(true);
            return;
        }

        serverSession.Close();
        serverOk.store(true);
        serverDone.store(true);
    });

    Socket clientSocket = Socket::CreateTcp();
    ASSERT_TRUE(clientSocket.IsValid());
    ASSERT_TRUE(clientSocket.Connect(kHost, port));

    TlsSession clientSession;
    TlsConfig clientConfig;
    clientConfig.enabled = true;
    clientConfig.caCertificatePath = certPath.string();
    clientConfig.serverName = kServerName;
    clientConfig.insecureSkipVerify = false;
    ASSERT_TRUE(clientSession.InitializeClient(clientSocket.NativeHandle(), clientConfig));
    serverInitializedFuture.wait();
    ASSERT_TRUE(clientSession.Handshake()) << "client TLS handshake failed";

    const uint8_t request[4] = {'A', 'W', 'N', 'W'};
    ASSERT_TRUE(clientSession.Send(request, sizeof(request)));

    uint8_t response[4] = {};
    ASSERT_TRUE(clientSession.RecvExact(response, sizeof(response)));
    EXPECT_EQ(response[0], 'A');
    EXPECT_EQ(response[1], 'W');
    EXPECT_EQ(response[2], 'N');
    EXPECT_EQ(response[3], 'X');

    clientSession.Close();
    clientSocket.Close();

    ASSERT_TRUE(WaitUntil([&]() { return serverDone.load(); }, std::chrono::seconds(5)));
    serverThread.join();
    EXPECT_TRUE(serverOk.load()) << serverError;

    listenSocket.Close();
    Socket::ShutdownNetworking();
}

#endif // ENGINE_ENABLE_TLS
