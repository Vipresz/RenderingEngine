// Burst join load test for the authoritative game server stack.
//
// Run manually (not part of default fast CI):
//   set ENGINE_RUN_JOIN_LOAD_TEST=1
//   EngineTests --gtest_filter=AuthoritativeServerFixture.HundredPlayerConcurrentJoinLatency
//
// Optional latency gates (milliseconds, p95):
//   set JOIN_LOAD_MAX_P95_MS=3000
//   set JOIN_LOAD_MAX_MOVEMENT_P95_MS=100

#include <gtest/gtest.h>

#include "TestHelpers.h"
#include "TestPort.h"
#include "core/network/MovementCommand.h"
#include "core/network/NetworkPayloads.h"
#include "core/network/ShardConfig.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetClient.h"
#include "network/NetServer.h"
#include "network/ServerMetrics.h"

#include <glm/glm.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Engine::Net::TransformStatePayload;
using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::ClientCallbacks;
using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;
using Engine::Platform::Networking::ServerTickMetrics;
namespace {

constexpr const char* kHost = "127.0.0.1";
constexpr int kPlayerCount = 100;
constexpr int kJoinRetryCount = 3;
constexpr uint64_t kMovementTickBase = 1'000'000;

bool JoinLoadEnabled() {
    const char* flag = std::getenv("ENGINE_RUN_JOIN_LOAD_TEST");
    return flag && flag[0] == '1' && flag[1] == '\0';
}

double OptionalMaxP95Ms() {
    const char* value = std::getenv("JOIN_LOAD_MAX_P95_MS");
    if (!value || value[0] == '\0') {
        return 0.0;
    }
    return std::atof(value);
}

double OptionalMaxMovementP95Ms() {
    const char* value = std::getenv("JOIN_LOAD_MAX_MOVEMENT_P95_MS");
    if (!value || value[0] == '\0') {
        return 0.0;
    }
    return std::atof(value);
}

struct JoinResult {
    bool success = false;
    double connectMs = 0.0;
    double loginMs = 0.0;
    double totalMs = 0.0;
    std::string error;
    std::unique_ptr<NetClient> client;
};

struct MovementRttResult {
    bool success = false;
    double rttMs = 0.0;
    std::string error;
};

class AuthoritativeServerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        tcpPort = Engine::Tests::AllocateEphemeralTestPort(50);
        udpPort = static_cast<uint16_t>(tcpPort + 1);

        Engine::Net::ShardConfig shard;
        shard.maxPlayers = 128;
        server.SetShardConfig(shard);
        server.SetAuthoritativeMode(true);

        simulator = std::make_unique<AuthoritativeGameSimulator>(server);
        simulator->SetSpawnPosition(glm::vec3(5.0f, 0.0f, 5.0f));
        simulator->SetTerrainSampler([](float /*x*/, float /*z*/) { return 0.0f; });

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
        server.SetGenericDataInterceptor([this](Engine::Net::UserId userId,
                                                Engine::Net::GenericDataPayload& payload) {
            return simulator && simulator->HandleGenericData(userId, payload);
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
        connectedClients.clear();
        running = false;
        if (tickThread.joinable()) {
            tickThread.join();
        }
        server.Stop();
        simulator.reset();
    }

    NetServer server;
    std::unique_ptr<AuthoritativeGameSimulator> simulator;
    std::vector<std::unique_ptr<NetClient>> connectedClients;
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;

private:
    std::atomic<bool> running{false};
    std::thread tickThread;
};

JoinResult JoinPlayer(uint16_t tcpPort, int playerIndex, std::atomic<bool>& startGate) {
    JoinResult result;
    const auto waitStart = std::chrono::steady_clock::now();
    while (!startGate.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() - waitStart > std::chrono::seconds(10)) {
            result.error = "start gate timeout";
            return result;
        }
        std::this_thread::yield();
    }

    const auto totalStart = std::chrono::steady_clock::now();
    const std::string displayName = "load_player_" + std::to_string(playerIndex);

    for (int attempt = 0; attempt < kJoinRetryCount; ++attempt) {
        auto client = std::make_unique<NetClient>();
        const auto connectStart = std::chrono::steady_clock::now();
        if (!client->Connect(kHost, tcpPort)) {
            client->Disconnect();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }
        result.connectMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - connectStart).count();

        const auto loginStart = std::chrono::steady_clock::now();
        if (!client->Login(displayName)) {
            client->Disconnect();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            continue;
        }

        result.loginMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loginStart).count();
        result.totalMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - totalStart).count();
        result.success = true;
        result.client = std::move(client);
        return result;
    }

    result.error = "connect/login failed after retries";
    result.totalMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - totalStart).count();
    return result;
}

bool RegisterClientUdp(NetClient& client, uint16_t udpPort, NetServer& server) {
    if (!client.ConnectUdp(kHost, udpPort)) {
        return false;
    }
    if (!client.RegisterUdpSession()) {
        return false;
    }
    return Engine::Tests::WaitUntil(
        [&]() {
            client.Poll();
            return server.HasUdpEndpointFor(client.GetUserId());
        },
        std::chrono::seconds(3));
}

bool WaitForAllPlayersSpawned(const NetServer& server,
                              const std::vector<std::unique_ptr<NetClient>>& clients) {
    return Engine::Tests::WaitUntil(
        [&]() {
            for (const std::unique_ptr<NetClient>& client : clients) {
                if (!client || !client->IsLoggedIn()) {
                    return false;
                }
                if (!server.GetServerAuthority().GetAuthoritative(client->GetUserId())) {
                    return false;
                }
            }
            return !clients.empty();
        },
        std::chrono::seconds(5));
}

MovementRttResult MeasureMovementRoundTrip(NetClient& client, uint64_t commandTick) {
    MovementRttResult result;
    const Engine::Net::UserId userId = client.GetUserId();
    std::atomic<bool> receivedEcho{false};

    ClientCallbacks callbacks;
    callbacks.onTransformState = [&](const TransformStatePayload& transform) {
        if (transform.userId == userId && transform.tick == commandTick) {
            receivedEcho.store(true, std::memory_order_release);
        }
    };
    client.SetCallbacks(std::move(callbacks));

    Engine::Net::MovementCommand command;
    command.tick = commandTick;
    command.deltaTime = 1.0f / 60.0f;
    command.moveX = 0.5f;
    command.moveZ = 0.0f;
    command.run = true;

    const auto sendStart = std::chrono::steady_clock::now();
    if (!client.SendMovementCommand(command)) {
        result.error = "SendMovementCommand failed";
        return result;
    }

    const bool echoed = Engine::Tests::WaitUntil(
        [&]() {
            client.Poll();
            return receivedEcho.load(std::memory_order_acquire);
        },
        std::chrono::seconds(3));

    result.rttMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sendStart).count();
    if (!echoed) {
        result.error = "authoritative transform echo timeout";
        return result;
    }

    result.success = true;
    return result;
}

void RecordLatencyProperties(const char* prefix, const Engine::Tests::LatencyStats& stats) {
    testing::Test::RecordProperty(std::string(prefix) + "_count", static_cast<int>(stats.sampleCount));
    testing::Test::RecordProperty(std::string(prefix) + "_min_ms", stats.minMs);
    testing::Test::RecordProperty(std::string(prefix) + "_mean_ms", stats.meanMs);
    testing::Test::RecordProperty(std::string(prefix) + "_p50_ms", stats.p50Ms);
    testing::Test::RecordProperty(std::string(prefix) + "_p95_ms", stats.p95Ms);
    testing::Test::RecordProperty(std::string(prefix) + "_p99_ms", stats.p99Ms);
    testing::Test::RecordProperty(std::string(prefix) + "_max_ms", stats.maxMs);
}

} // namespace

TEST_F(AuthoritativeServerFixture, HundredPlayerConcurrentJoinLatency) {
    if (!JoinLoadEnabled()) {
        GTEST_SKIP() << "Set ENGINE_RUN_JOIN_LOAD_TEST=1 to run the 100-player join load test";
    }

    std::vector<JoinResult> results(static_cast<size_t>(kPlayerCount));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(kPlayerCount));

    std::atomic<bool> startGate{false};
    std::atomic<int> workersReady{0};
    std::mutex resultsMutex;

    for (int playerIndex = 0; playerIndex < kPlayerCount; ++playerIndex) {
        workers.emplace_back([&, playerIndex]() {
            workersReady.fetch_add(1, std::memory_order_relaxed);
            JoinResult result = JoinPlayer(tcpPort, playerIndex, startGate);
            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                results[static_cast<size_t>(playerIndex)] = std::move(result);
            }
        });
    }

    ASSERT_TRUE(Engine::Tests::WaitUntil(
        [&]() { return workersReady.load(std::memory_order_acquire) >= kPlayerCount; },
        std::chrono::seconds(5)))
        << "Worker threads failed to start";

    const auto burstStart = std::chrono::steady_clock::now();
    startGate.store(true, std::memory_order_release);

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    const double burstWallMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - burstStart).count();

    connectedClients.reserve(static_cast<size_t>(kPlayerCount));
    size_t successCount = 0;
    std::vector<double> connectMs;
    std::vector<double> loginMs;
    std::vector<double> totalMs;
    connectMs.reserve(static_cast<size_t>(kPlayerCount));
    loginMs.reserve(static_cast<size_t>(kPlayerCount));
    totalMs.reserve(static_cast<size_t>(kPlayerCount));

    for (JoinResult& result : results) {
        if (!result.success) {
            continue;
        }
        ++successCount;
        connectMs.push_back(result.connectMs);
        loginMs.push_back(result.loginMs);
        totalMs.push_back(result.totalMs);
        connectedClients.push_back(std::move(result.client));
    }

    for (size_t i = 0; i < results.size(); ++i) {
        if (!results[i].success) {
            ADD_FAILURE() << "Player " << i << " join failed: " << results[i].error;
        }
    }

    EXPECT_EQ(successCount, static_cast<size_t>(kPlayerCount));
    EXPECT_EQ(server.GetLoggedInClientCount(), static_cast<size_t>(kPlayerCount));

    const Engine::Tests::LatencyStats connectStats = Engine::Tests::ComputeLatencyStats(connectMs);
    const Engine::Tests::LatencyStats loginStats = Engine::Tests::ComputeLatencyStats(loginMs);
    const Engine::Tests::LatencyStats totalStats = Engine::Tests::ComputeLatencyStats(totalMs);

    RecordLatencyProperties("connect", connectStats);
    RecordLatencyProperties("login", loginStats);
    RecordLatencyProperties("total_join", totalStats);
    testing::Test::RecordProperty("burst_wall_ms", burstWallMs);
    testing::Test::RecordProperty("successful_joins", static_cast<int>(successCount));

    std::cout << "[JoinLoad] burst_wall=" << burstWallMs << "ms"
              << " connect {" << Engine::Tests::FormatLatencyStats(connectStats) << "}"
              << " login {" << Engine::Tests::FormatLatencyStats(loginStats) << "}"
              << " total {" << Engine::Tests::FormatLatencyStats(totalStats) << "}"
              << std::endl;

    const double maxP95Ms = OptionalMaxP95Ms();
    if (maxP95Ms > 0.0) {
        EXPECT_LE(totalStats.p95Ms, maxP95Ms)
            << "Join p95 exceeded JOIN_LOAD_MAX_P95_MS (" << maxP95Ms << "ms)";
    }

    // Phase 2: register UDP and wait for authoritative spawns with full population online.
    size_t udpRegistered = 0;
    for (std::unique_ptr<NetClient>& client : connectedClients) {
        if (client && RegisterClientUdp(*client, udpPort, server)) {
            ++udpRegistered;
        }
    }
    EXPECT_EQ(udpRegistered, static_cast<size_t>(kPlayerCount))
        << "All clients should register UDP after join";

    ASSERT_TRUE(WaitForAllPlayersSpawned(server, connectedClients))
        << "Timed out waiting for all players to spawn on the authoritative server";

    // Phase 3: movement round-trip under load (all players send at once).
    std::vector<MovementRttResult> movementResults(static_cast<size_t>(kPlayerCount));
    std::vector<std::thread> movementWorkers;
    movementWorkers.reserve(static_cast<size_t>(kPlayerCount));
    std::atomic<bool> movementStartGate{false};
    std::atomic<int> movementWorkersReady{0};
    std::mutex movementResultsMutex;

    for (int playerIndex = 0; playerIndex < kPlayerCount; ++playerIndex) {
        movementWorkers.emplace_back([&, playerIndex]() {
            movementWorkersReady.fetch_add(1, std::memory_order_relaxed);
            const auto waitStart = std::chrono::steady_clock::now();
            while (!movementStartGate.load(std::memory_order_acquire)) {
                if (std::chrono::steady_clock::now() - waitStart > std::chrono::seconds(10)) {
                    MovementRttResult timeout;
                    timeout.error = "movement start gate timeout";
                    std::lock_guard<std::mutex> lock(movementResultsMutex);
                    movementResults[static_cast<size_t>(playerIndex)] = std::move(timeout);
                    return;
                }
                std::this_thread::yield();
            }

            NetClient* client = connectedClients[static_cast<size_t>(playerIndex)].get();
            if (!client) {
                MovementRttResult missing;
                missing.error = "missing client";
                std::lock_guard<std::mutex> lock(movementResultsMutex);
                movementResults[static_cast<size_t>(playerIndex)] = std::move(missing);
                return;
            }

            MovementRttResult sample =
                MeasureMovementRoundTrip(*client, kMovementTickBase + static_cast<uint64_t>(playerIndex));
            std::lock_guard<std::mutex> lock(movementResultsMutex);
            movementResults[static_cast<size_t>(playerIndex)] = std::move(sample);
        });
    }

    ASSERT_TRUE(Engine::Tests::WaitUntil(
        [&]() { return movementWorkersReady.load(std::memory_order_acquire) >= kPlayerCount; },
        std::chrono::seconds(5)))
        << "Movement worker threads failed to start";

    const auto movementBurstStart = std::chrono::steady_clock::now();
    movementStartGate.store(true, std::memory_order_release);
    for (std::thread& worker : movementWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    const double movementBurstWallMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - movementBurstStart)
            .count();

    std::vector<double> movementRttMs;
    movementRttMs.reserve(static_cast<size_t>(kPlayerCount));
    size_t movementSuccessCount = 0;
    for (size_t i = 0; i < movementResults.size(); ++i) {
        if (!movementResults[i].success) {
            ADD_FAILURE() << "Player " << i << " movement RTT failed: " << movementResults[i].error;
            continue;
        }
        ++movementSuccessCount;
        movementRttMs.push_back(movementResults[i].rttMs);
    }

    EXPECT_EQ(movementSuccessCount, static_cast<size_t>(kPlayerCount));

    const Engine::Tests::LatencyStats movementStats = Engine::Tests::ComputeLatencyStats(movementRttMs);
    RecordLatencyProperties("movement_rtt", movementStats);
    testing::Test::RecordProperty("movement_burst_wall_ms", movementBurstWallMs);
    testing::Test::RecordProperty("movement_success_count", static_cast<int>(movementSuccessCount));

    std::cout << "[ServerLoad] players=" << kPlayerCount << " movement_burst_wall=" << movementBurstWallMs
              << "ms movement_rtt {" << Engine::Tests::FormatLatencyStats(movementStats) << "}" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const ServerTickMetrics tickMetrics = simulator->GetLastTickMetrics();
    const ServerTickMetrics sessionMetrics = simulator->GetSessionMetrics();
    testing::Test::RecordProperty("server_tick_ms", tickMetrics.tickMs);
    testing::Test::RecordProperty("server_connected_players", static_cast<int>(tickMetrics.connectedPlayers));
    testing::Test::RecordProperty("server_movement_cmds", static_cast<int>(sessionMetrics.movementCommandsProcessed));
    testing::Test::RecordProperty("server_movement_dropped", static_cast<int>(sessionMetrics.movementCommandsDropped));
    std::cout << "[ServerMetrics] tick_ms=" << tickMetrics.tickMs
              << " players=" << tickMetrics.connectedPlayers
              << " movement_cmds=" << sessionMetrics.movementCommandsProcessed
              << " movement_dropped=" << sessionMetrics.movementCommandsDropped
              << " dirty_snapshots=" << sessionMetrics.dirtySnapshotsSent
              << " visibility_events=" << sessionMetrics.visibilityEvents << std::endl;

    const double maxMovementP95Ms = OptionalMaxMovementP95Ms();
    if (maxMovementP95Ms > 0.0) {
        EXPECT_LE(movementStats.p95Ms, maxMovementP95Ms)
            << "Movement RTT p95 exceeded JOIN_LOAD_MAX_MOVEMENT_P95_MS (" << maxMovementP95Ms << "ms)";
    }
}
