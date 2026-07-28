#include <gtest/gtest.h>

#include "TestPort.h"
#include "TestHelpers.h"
#include "core/network/ZoneScaleConfig.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetClient.h"
#include "network/NetServer.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;

namespace {

constexpr const char* kHost = "127.0.0.1";
constexpr int kConcurrentJoinAttempts = 12;
constexpr int kShardSoftCap = 8;

Engine::Net::ZoneScaleConfig MakeShardFirstStressConfig() {
    Engine::Net::ZoneScaleConfig config;
    config.zoneId = "shard_stress_arena";
    config.shard.shardId = 99;
    config.shard.maxPlayers = kShardSoftCap;
    config.shard.redirectHost = "127.0.0.1";
    config.shard.redirectTcpPort = 17999;
    config.shard.redirectUdpPort = 18000;
    config.scalePolicy.overflowPolicy = Engine::Net::OverflowPolicy::ShardFirst;
    config.scalePolicy.shardSoftCap = kShardSoftCap;
    config.scalePolicy.shardHardCap = kShardSoftCap;
    config.scalePolicy.cellLayerDensityThreshold = 4;
    return config;
}

Engine::Net::ZoneScaleConfig MakeLayeringStressConfig() {
    Engine::Net::ZoneScaleConfig config;
    config.zoneId = "layer_stress_arena";
    config.shard.shardId = 2;
    config.shard.maxPlayers = 64;
    config.scalePolicy.overflowPolicy = Engine::Net::OverflowPolicy::LayerThenShard;
    config.scalePolicy.shardSoftCap = 64;
    config.scalePolicy.shardHardCap = 64;
    config.scalePolicy.cellLayerDensityThreshold = 4;
    return config;
}

} // namespace

class NetServerScalePolicyMultiUserTest : public ::testing::Test {
protected:
    NetServer server;
    std::unique_ptr<AuthoritativeGameSimulator> simulator;
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;
    std::atomic<bool> running{false};
    std::thread tickThread;

    void SetUp() override {
        tcpPort = Engine::Tests::AllocateEphemeralTestPort(60);
        udpPort = static_cast<uint16_t>(tcpPort + 1);

        server.ApplyZoneScaleConfig(MakeShardFirstStressConfig());
        server.SetAuthoritativeMode(true);

        simulator = std::make_unique<AuthoritativeGameSimulator>(server);
        simulator->SetSpawnPosition(glm::vec3(0.0f, 0.0f, 0.0f));

        server.SetClientLoggedInCallback([this](Engine::Net::UserId userId, const std::string& displayName) {
            if (simulator) {
                simulator->OnClientLoggedIn(userId, displayName);
            }
        });
        server.SetClientDisconnectedCallback([this](Engine::Net::UserId userId) {
            if (simulator) {
                simulator->OnClientDisconnected(userId);
            }
        });
        server.SetMovementCommandInterceptor([this](Engine::Net::UserId userId,
                                                    Engine::Net::MovementCommand& command) {
            return simulator && simulator->HandleMovementCommand(userId, command);
        });

        ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));

        running = true;
        tickThread = std::thread([this]() {
            constexpr float kTickSeconds = 1.0f / 60.0f;
            while (running.load(std::memory_order_acquire)) {
                if (simulator) {
                    simulator->Update(kTickSeconds);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        });
    }

    void TearDown() override {
        running = false;
        if (tickThread.joinable()) {
            tickThread.join();
        }
        server.Stop();
        simulator.reset();
    }
};

TEST_F(NetServerScalePolicyMultiUserTest, ShardingCapsConcurrentLoginsAtSoftCap) {
    std::vector<bool> loginSuccess(static_cast<size_t>(kConcurrentJoinAttempts), false);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(kConcurrentJoinAttempts));
    std::atomic<int> workersReady{0};
    std::atomic<bool> startGate{false};
    std::mutex resultsMutex;

    for (int index = 0; index < kConcurrentJoinAttempts; ++index) {
        workers.emplace_back([&, index]() {
            workersReady.fetch_add(1, std::memory_order_relaxed);
            while (!startGate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            NetClient client;
            bool success = false;
            if (client.Connect(kHost, tcpPort)) {
                success = client.Login("player_" + std::to_string(index));
            }

            std::lock_guard<std::mutex> lock(resultsMutex);
            loginSuccess[static_cast<size_t>(index)] = success;
        });
    }

    ASSERT_TRUE(Engine::Tests::WaitUntil(
        [&]() { return workersReady.load(std::memory_order_acquire) >= kConcurrentJoinAttempts; },
        std::chrono::seconds(5)))
        << "Concurrent join workers failed to start";

    startGate.store(true, std::memory_order_release);
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    size_t successCount = 0;
    for (bool success : loginSuccess) {
        if (success) {
            ++successCount;
        }
    }

    EXPECT_EQ(successCount, static_cast<size_t>(kShardSoftCap))
        << "ShardFirst soft cap should allow exactly " << kShardSoftCap << " concurrent logins";
    EXPECT_EQ(server.GetLoggedInClientCount(), static_cast<size_t>(kShardSoftCap));
    EXPECT_GE(kConcurrentJoinAttempts - static_cast<int>(successCount), 1)
        << "Expected at least one login rejection once shard soft cap is reached";
}

TEST_F(NetServerScalePolicyMultiUserTest, LayeringActivatesWhenCrowdSharesSpawnCell) {
    server.ApplyZoneScaleConfig(MakeLayeringStressConfig());

    std::vector<std::unique_ptr<NetClient>> clients;
    clients.reserve(static_cast<size_t>(kShardSoftCap));

    for (int index = 0; index < kShardSoftCap; ++index) {
        auto client = std::make_unique<NetClient>();
        ASSERT_TRUE(client->Connect(kHost, tcpPort));
        ASSERT_TRUE(client->Login("crowd_" + std::to_string(index)));
        clients.push_back(std::move(client));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    const auto metrics = simulator->GetLastTickMetrics();
    EXPECT_GE(metrics.cellDensityMax, 4u)
        << "Crowded spawn grid should exceed layering density threshold (4)";
    EXPECT_TRUE(metrics.layeringActive)
        << "Layering should activate once cell density exceeds threshold (4)";
    EXPECT_FALSE(metrics.shardRedirectRecommended)
        << "LayerThenShard should prefer layering below soft cap with healthy ticks";

    std::cout << "[ScalePolicyMultiUser] players=" << metrics.connectedPlayers
              << " cell_density_max=" << metrics.cellDensityMax
              << " layering_active=" << metrics.layeringActive
              << " shard_redirect=" << metrics.shardRedirectRecommended << std::endl;
}
