#include "Logger.h"
#include "core/network/NetTypes.h"
#include "network/NetClient.h"
#include "network/NetServer.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using Engine::Net::kDefaultPort;
using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;
using EngineLogging::Logger;

namespace {

void PrintUsage(const char* program) {
    std::cout << "Usage:\n"
              << "  " << program << " server [port]\n"
              << "  " << program << " client [host] [port] [displayName]\n";
}

} // namespace

int main(int argc, char** argv) {
    EngineLogging::Logger& logger = EngineLogging::Logger::GetInstance();
    logger.Initialize("network_example.log");
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);

    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    const std::string mode = argv[1];
    const uint16_t port = (argc >= 3) ? static_cast<uint16_t>(std::atoi(argv[2])) : kDefaultPort;

    if (mode == "server") {
        NetServer server;
        if (!server.Start("0.0.0.0", port)) {
            LOG_ERROR("NetworkExample", "Failed to start server");
            return 1;
        }

        LOG_INFO_FMT("NetworkExample", "Server running on port %u. Press Ctrl+C to exit.", port);
        while (server.IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        server.Stop();
        return 0;
    }

    if (mode == "client") {
        const std::string host = (argc >= 3) ? argv[2] : "127.0.0.1";
        const uint16_t clientPort = (argc >= 4) ? static_cast<uint16_t>(std::atoi(argv[3])) : kDefaultPort;
        const std::string displayName = (argc >= 5) ? argv[4] : "Player";

        NetClient client;
        if (!client.Connect(host, clientPort) || !client.Login(displayName)) {
            LOG_ERROR("NetworkExample", "Failed to connect/login");
            return 1;
        }

        LOG_INFO_FMT("NetworkExample", "Logged in as user %u", client.GetUserId());

        Engine::Net::TransformStatePayload transform;
        transform.position = {0.0f, 1.0f, 0.0f};
        transform.rotationDegrees = {0.0f, 45.0f, 0.0f};

        for (int i = 0; i < 100; ++i) {
            client.Poll();
            transform.tick = static_cast<uint64_t>(i);
            transform.position.x += 0.1f;
            client.SendTransformState(transform);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        client.Disconnect();
        return 0;
    }

    PrintUsage(argv[0]);
    return 1;
}
