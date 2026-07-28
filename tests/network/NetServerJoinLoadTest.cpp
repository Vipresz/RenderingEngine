// Burst join load test for the authoritative game server stack.
//
// Run manually (not part of default fast CI):
//   set ENGINE_RUN_JOIN_LOAD_TEST=1
//   EngineTests --gtest_filter=AuthoritativeServerFixture.HundredPlayerConcurrentJoinLatency
//
// Stress profile sweep (64 / 256 / 1048 players):
//   .\scripts\run_join_load_stress_profile.ps1
//
// Optional latency gates (milliseconds, p95):
//   set JOIN_LOAD_MAX_P95_MS=3000
//   set JOIN_LOAD_MAX_MOVEMENT_P95_MS=100
//
// Optional player count override (default 100 for the legacy test):
//   set JOIN_LOAD_PLAYER_COUNT=256

#include <gtest/gtest.h>

#include "TestHelpers.h"
#include "TestPort.h"
#include "core/network/JoinAdmission.h"
#include "core/network/MovementCommand.h"
#include "core/network/NetworkPayloads.h"
#include "core/network/ShardConfig.h"
#include "core/network/ScalePolicyConfig.h"
#include "core/network/ZoneScaleConfig.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetClient.h"
#include "network/NetServer.h"
#include "network/ServerMetrics.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using Engine::Net::MovementAckPayload;
using Engine::Net::TransformStatePayload;
using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::ClientCallbacks;
using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;
using Engine::Platform::Networking::ServerTickMetrics;
namespace {

constexpr const char* kHost = "127.0.0.1";
constexpr int kDefaultPlayerCount = 100;
constexpr uint64_t kMovementTickBase = 1'000'000;

int JoinLoadPlayerCount() {
    const char* value = std::getenv("JOIN_LOAD_PLAYER_COUNT");
    if (!value || value[0] == '\0') {
        return kDefaultPlayerCount;
    }
    const int parsed = std::atoi(value);
    return parsed > 0 ? parsed : kDefaultPlayerCount;
}

int JoinRetryCountForPlayers(int playerCount) {
    if (playerCount >= 512) {
        return 8;
    }
    if (playerCount >= 128) {
        return 5;
    }
    return 3;
}

std::chrono::seconds JoinGateTimeoutForPlayers(int playerCount) {
    return std::chrono::seconds(std::max(10, playerCount / 16 + 10));
}

std::chrono::seconds SpawnWaitTimeoutForPlayers(int playerCount) {
    return std::chrono::seconds(std::max(5, playerCount / 12 + 5));
}

std::chrono::seconds MovementEchoTimeoutForPlayers(int playerCount) {
    return std::chrono::seconds(std::max(3, playerCount / 32 + 3));
}

bool StressProfileEnabled() {
    const char* flag = std::getenv("JOIN_LOAD_STRESS_PROFILE");
    return flag && flag[0] == '1' && flag[1] == '\0';
}

bool AllowPartialSuccess() {
    const char* flag = std::getenv("JOIN_LOAD_ALLOW_PARTIAL_SUCCESS");
    return flag && flag[0] == '1' && flag[1] == '\0';
}

std::string ResolveJoinLoadScaleConfigPath() {
    if (const char* envPath = std::getenv("JOIN_LOAD_SCALE_CONFIG")) {
        if (envPath[0] != '\0') {
            return envPath;
        }
    }

#ifdef ENGINE_CONFIG_DIR
    {
        const std::string bundledPath = std::string(ENGINE_CONFIG_DIR) + "/join_load_scale.json";
        std::ifstream bundled(bundledPath);
        if (bundled.good()) {
            return bundledPath;
        }
    }
#endif

    const char* candidates[] = {
        "config/join_load_scale.json",
        "../config/join_load_scale.json",
        "../../config/join_load_scale.json",
        "../../../config/join_load_scale.json",
    };
    for (const char* path : candidates) {
        std::ifstream input(path);
        if (input.good()) {
            return path;
        }
    }
    return {};
}

bool ApplyJoinLoadZoneConfig(NetServer& server, std::string* outConfigPath = nullptr) {
    const std::string configPath = ResolveJoinLoadScaleConfigPath();
    if (configPath.empty()) {
        return false;
    }

    Engine::Net::ZoneScaleConfig zoneConfig;
    if (!Engine::Net::ZoneScaleConfig::TryLoadFromFile(configPath, zoneConfig)) {
        return false;
    }

    server.ApplyZoneScaleConfig(zoneConfig);
    if (outConfigPath) {
        *outConfigPath = configPath;
    }

    std::cout << "[JoinLoadConfig] path=" << configPath << " zone=" << zoneConfig.zoneId
              << " shard_soft_cap=" << zoneConfig.scalePolicy.shardSoftCap
              << " shard_hard_cap=" << zoneConfig.scalePolicy.shardHardCap
              << " layer_density_threshold=" << zoneConfig.scalePolicy.cellLayerDensityThreshold
              << " overflow_policy=";
    switch (zoneConfig.scalePolicy.overflowPolicy) {
        case Engine::Net::OverflowPolicy::LayerFirst:
            std::cout << "LayerFirst";
            break;
        case Engine::Net::OverflowPolicy::ShardFirst:
            std::cout << "ShardFirst";
            break;
        case Engine::Net::OverflowPolicy::LayerThenShard:
        default:
            std::cout << "LayerThenShard";
            break;
    }
    std::cout << std::endl;
    return true;
}

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

struct JoinLoadScenarioResult {
    int playerCountTarget = 0;
    size_t successCount = 0;
    size_t movementSuccessCount = 0;
    double burstWallMs = 0.0;
    double movementBurstWallMs = 0.0;
    Engine::Tests::LatencyStats connectStats{};
    Engine::Tests::LatencyStats loginStats{};
    Engine::Tests::LatencyStats totalStats{};
    Engine::Tests::LatencyStats movementStats{};
    ServerTickMetrics tickMetrics{};
    ServerTickMetrics sessionMetrics{};
    uint32_t loginDropped = 0;
    bool passed = false;
};

JoinResult JoinPlayer(uint16_t tcpPort,
                      int playerIndex,
                      int playerCount,
                      std::atomic<bool>& startGate,
                      int joinRetryCount);
bool RegisterClientUdp(NetClient& client, uint16_t udpPort, NetServer& server);
bool WaitForAllPlayersSpawned(const NetServer& server,
                              const std::vector<std::unique_ptr<NetClient>>& clients,
                              std::chrono::seconds timeout);
MovementRttResult MeasureMovementRoundTrip(NetClient& client,
                                           uint64_t commandTick,
                                           std::chrono::seconds echoTimeout);
void RecordLatencyProperties(const char* prefix, const Engine::Tests::LatencyStats& stats);
std::string FormatLatencyStatsJson(const Engine::Tests::LatencyStats& stats);
void EmitJoinLoadProfile(const JoinLoadScenarioResult& result);

class AuthoritativeServerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        playerCount = JoinLoadPlayerCount();
        joinRetryCount = JoinRetryCountForPlayers(playerCount);

        tcpPort = Engine::Tests::AllocateEphemeralTestPort(50);
        udpPort = static_cast<uint16_t>(tcpPort + 1);

        Engine::Net::ShardConfig shard;
        shard.maxPlayers = static_cast<uint32_t>(std::max(playerCount, 128));
        server.SetShardConfig(shard);

        std::string configPath;
        ASSERT_TRUE(ApplyJoinLoadZoneConfig(server, &configPath))
            << "Failed to load join load scale config. Set JOIN_LOAD_SCALE_CONFIG to "
               "RenderingEngine/config/join_load_scale.json";
        testing::Test::RecordProperty("join_load_scale_config", configPath);
        server.SetAuthoritativeMode(true);

        if (JoinLoadEnabled() || StressProfileEnabled()) {
            const float loginBurst = static_cast<float>(std::max(playerCount * 2, 256));
            server.SetLoginRateLimit(10000.0f, loginBurst);
            server.SetMovementRateLimit(10000.0f, 500.0f);
        }

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
    int playerCount = kDefaultPlayerCount;
    int joinRetryCount = 3;

    JoinLoadScenarioResult RunJoinLoadScenario() {
        JoinLoadScenarioResult scenario;
        scenario.playerCountTarget = playerCount;

        const int joinRetryCountLocal = joinRetryCount;
        const auto joinGateTimeout = JoinGateTimeoutForPlayers(playerCount);
        const auto spawnWaitTimeout = SpawnWaitTimeoutForPlayers(playerCount);
        const auto movementEchoTimeout = MovementEchoTimeoutForPlayers(playerCount);

        std::vector<JoinResult> results(static_cast<size_t>(playerCount));
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(playerCount));

        std::atomic<bool> startGate{false};
        std::atomic<int> workersReady{0};
        std::mutex resultsMutex;

        for (int playerIndex = 0; playerIndex < playerCount; ++playerIndex) {
            workers.emplace_back([&, playerIndex]() {
                workersReady.fetch_add(1, std::memory_order_relaxed);
                JoinResult result = JoinPlayer(tcpPort, playerIndex, playerCount, startGate, joinRetryCountLocal);
                {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    results[static_cast<size_t>(playerIndex)] = std::move(result);
                }
            });
        }

        if (!Engine::Tests::WaitUntil(
                [&]() { return workersReady.load(std::memory_order_acquire) >= playerCount; },
                joinGateTimeout)) {
            ADD_FAILURE() << "Worker threads failed to start";
            EmitJoinLoadProfile(scenario);
            return scenario;
        }

        const auto burstStart = std::chrono::steady_clock::now();
        startGate.store(true, std::memory_order_release);

        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        scenario.burstWallMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - burstStart).count();

        connectedClients.reserve(static_cast<size_t>(playerCount));
        std::vector<double> connectMs;
        std::vector<double> loginMs;
        std::vector<double> totalMs;
        connectMs.reserve(static_cast<size_t>(playerCount));
        loginMs.reserve(static_cast<size_t>(playerCount));
        totalMs.reserve(static_cast<size_t>(playerCount));

        for (JoinResult& result : results) {
            if (!result.success) {
                continue;
            }
            ++scenario.successCount;
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

        scenario.connectStats = Engine::Tests::ComputeLatencyStats(connectMs);
        scenario.loginStats = Engine::Tests::ComputeLatencyStats(loginMs);
        scenario.totalStats = Engine::Tests::ComputeLatencyStats(totalMs);

        RecordLatencyProperties("connect", scenario.connectStats);
        RecordLatencyProperties("login", scenario.loginStats);
        RecordLatencyProperties("total_join", scenario.totalStats);
        testing::Test::RecordProperty("burst_wall_ms", scenario.burstWallMs);
        testing::Test::RecordProperty("successful_joins", static_cast<int>(scenario.successCount));
        testing::Test::RecordProperty("player_count_target", playerCount);

        std::cout << "[JoinLoad] players=" << playerCount << " burst_wall=" << scenario.burstWallMs << "ms"
                  << " connect {" << Engine::Tests::FormatLatencyStats(scenario.connectStats) << "}"
                  << " login {" << Engine::Tests::FormatLatencyStats(scenario.loginStats) << "}"
                  << " total {" << Engine::Tests::FormatLatencyStats(scenario.totalStats) << "}"
                  << std::endl;

        const double maxP95Ms = OptionalMaxP95Ms();
        if (maxP95Ms > 0.0) {
            EXPECT_LE(scenario.totalStats.p95Ms, maxP95Ms)
                << "Join p95 exceeded JOIN_LOAD_MAX_P95_MS (" << maxP95Ms << "ms)";
        }

        size_t udpRegistered = 0;
        for (std::unique_ptr<NetClient>& client : connectedClients) {
            if (client && RegisterClientUdp(*client, udpPort, server)) {
                ++udpRegistered;
            }
        }
        EXPECT_EQ(udpRegistered, scenario.successCount) << "All joined clients should register UDP";

        if (!connectedClients.empty()) {
            if (!WaitForAllPlayersSpawned(server, connectedClients, spawnWaitTimeout)) {
                ADD_FAILURE() << "Timed out waiting for players to spawn on the authoritative server";
                EmitJoinLoadProfile(scenario);
                return scenario;
            }
        }

        std::vector<MovementRttResult> movementResults(static_cast<size_t>(playerCount));
        std::vector<std::thread> movementWorkers;
        movementWorkers.reserve(static_cast<size_t>(playerCount));
        std::atomic<bool> movementStartGate{false};
        std::atomic<int> movementWorkersReady{0};
        std::mutex movementResultsMutex;

        for (int playerIndex = 0; playerIndex < playerCount; ++playerIndex) {
            movementWorkers.emplace_back([&, playerIndex]() {
                movementWorkersReady.fetch_add(1, std::memory_order_relaxed);
                const auto waitStart = std::chrono::steady_clock::now();
                while (!movementStartGate.load(std::memory_order_acquire)) {
                    if (std::chrono::steady_clock::now() - waitStart > joinGateTimeout) {
                        MovementRttResult timeout;
                        timeout.error = "movement start gate timeout";
                        std::lock_guard<std::mutex> lock(movementResultsMutex);
                        movementResults[static_cast<size_t>(playerIndex)] = std::move(timeout);
                        return;
                    }
                    std::this_thread::yield();
                }

                NetClient* client = nullptr;
                if (playerIndex >= 0 && static_cast<size_t>(playerIndex) < connectedClients.size()) {
                    client = connectedClients[static_cast<size_t>(playerIndex)].get();
                }
                if (!client) {
                    MovementRttResult missing;
                    missing.error = "missing client";
                    std::lock_guard<std::mutex> lock(movementResultsMutex);
                    movementResults[static_cast<size_t>(playerIndex)] = std::move(missing);
                    return;
                }

                MovementRttResult sample = MeasureMovementRoundTrip(
                    *client, kMovementTickBase + static_cast<uint64_t>(playerIndex), movementEchoTimeout);
                std::lock_guard<std::mutex> lock(movementResultsMutex);
                movementResults[static_cast<size_t>(playerIndex)] = std::move(sample);
            });
        }

        if (!Engine::Tests::WaitUntil(
                [&]() { return movementWorkersReady.load(std::memory_order_acquire) >= playerCount; },
                joinGateTimeout)) {
            ADD_FAILURE() << "Movement worker threads failed to start";
            EmitJoinLoadProfile(scenario);
            return scenario;
        }

        const auto movementBurstStart = std::chrono::steady_clock::now();
        movementStartGate.store(true, std::memory_order_release);
        for (std::thread& worker : movementWorkers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        scenario.movementBurstWallMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - movementBurstStart)
                .count();

        std::vector<double> movementRttMs;
        movementRttMs.reserve(static_cast<size_t>(playerCount));
        for (size_t i = 0; i < movementResults.size(); ++i) {
            if (!movementResults[i].success) {
                ADD_FAILURE() << "Player " << i << " movement RTT failed: " << movementResults[i].error;
                continue;
            }
            ++scenario.movementSuccessCount;
            movementRttMs.push_back(movementResults[i].rttMs);
        }

        scenario.movementStats = Engine::Tests::ComputeLatencyStats(movementRttMs);
        RecordLatencyProperties("movement_rtt", scenario.movementStats);
        testing::Test::RecordProperty("movement_burst_wall_ms", scenario.movementBurstWallMs);
        testing::Test::RecordProperty("movement_success_count", static_cast<int>(scenario.movementSuccessCount));

        std::cout << "[ServerLoad] players=" << playerCount
                  << " movement_burst_wall=" << scenario.movementBurstWallMs << "ms movement_rtt {"
                  << Engine::Tests::FormatLatencyStats(scenario.movementStats) << "}" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        scenario.tickMetrics = simulator->GetLastTickMetrics();
        scenario.sessionMetrics = simulator->GetSessionMetrics();
        scenario.tickMetrics.tickMsMax = scenario.sessionMetrics.sessionTickMsMax;
        scenario.loginDropped = server.ConsumeLoginAttemptsDropped();

        testing::Test::RecordProperty("server_tick_ms", scenario.tickMetrics.tickMs);
        testing::Test::RecordProperty("server_connected_players",
                                      static_cast<int>(scenario.tickMetrics.connectedPlayers));
        testing::Test::RecordProperty("server_movement_cmds",
                                      static_cast<int>(scenario.sessionMetrics.movementCommandsProcessed));
        testing::Test::RecordProperty("server_movement_dropped",
                                      static_cast<int>(scenario.sessionMetrics.movementCommandsDropped));
        std::cout << "[ServerMetrics] tick_ms=" << scenario.tickMetrics.tickMs
                  << " tick_p95_ms=" << scenario.tickMetrics.tickP95Ms
                  << " tick_max_ms=" << scenario.tickMetrics.tickMsMax
                  << " players=" << scenario.tickMetrics.connectedPlayers
                  << " cell_density_max=" << scenario.tickMetrics.cellDensityMax
                  << " layering_active=" << (scenario.tickMetrics.layeringActive ? 1 : 0)
                  << " shard_redirect=" << (scenario.tickMetrics.shardRedirectRecommended ? 1 : 0)
                  << " movement_cmds=" << scenario.sessionMetrics.movementCommandsProcessed
                  << " movement_dropped=" << scenario.sessionMetrics.movementCommandsDropped
                  << " peer_xforms_sent=" << scenario.tickMetrics.peerTransformsSent
                  << " repl_queue=" << scenario.tickMetrics.replicationQueueDepth
                  << " dirty_snapshots=" << scenario.sessionMetrics.dirtySnapshotsSent
                  << " visibility_events=" << scenario.sessionMetrics.visibilityEvents
                  << " login_dropped=" << scenario.loginDropped << std::endl;

        const double maxMovementP95Ms = OptionalMaxMovementP95Ms();
        if (maxMovementP95Ms > 0.0) {
            EXPECT_LE(scenario.movementStats.p95Ms, maxMovementP95Ms)
                << "Movement RTT p95 exceeded JOIN_LOAD_MAX_MOVEMENT_P95_MS (" << maxMovementP95Ms
                << "ms)";
        }

        const bool requireFullSuccess = !AllowPartialSuccess();
        if (requireFullSuccess) {
            EXPECT_EQ(scenario.successCount, static_cast<size_t>(playerCount));
            EXPECT_EQ(server.GetLoggedInClientCount(), static_cast<size_t>(playerCount));
            EXPECT_EQ(scenario.movementSuccessCount, static_cast<size_t>(playerCount));
        }

        scenario.passed = scenario.successCount == static_cast<size_t>(playerCount) &&
                          scenario.movementSuccessCount == static_cast<size_t>(playerCount);
        EmitJoinLoadProfile(scenario);
        return scenario;
    }

private:
    std::atomic<bool> running{false};
    std::thread tickThread;
};

JoinResult JoinPlayer(uint16_t tcpPort,
                      int playerIndex,
                      int playerCount,
                      std::atomic<bool>& startGate,
                      int joinRetryCount) {
    JoinResult result;
    const auto waitStart = std::chrono::steady_clock::now();
    while (!startGate.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() - waitStart > std::chrono::seconds(10)) {
            result.error = "start gate timeout";
            return result;
        }
        std::this_thread::yield();
    }

    Engine::Net::SleepJoinStagger(playerIndex, playerCount);

    const auto totalStart = std::chrono::steady_clock::now();
    const std::string displayName = "load_player_" + std::to_string(playerIndex);

    for (int attempt = 0; attempt < joinRetryCount; ++attempt) {
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
                              const std::vector<std::unique_ptr<NetClient>>& clients,
                              std::chrono::seconds timeout) {
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
        timeout);
}

MovementRttResult MeasureMovementRoundTrip(NetClient& client,
                                           uint64_t commandTick,
                                           std::chrono::seconds echoTimeout) {
    MovementRttResult result;
    const Engine::Net::UserId userId = client.GetUserId();
    std::atomic<bool> receivedEcho{false};

    ClientCallbacks callbacks;
    callbacks.onMovementAck = [&](const MovementAckPayload& ack) {
        if (ack.userId == userId && ack.tick == commandTick) {
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
        echoTimeout);

    result.rttMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sendStart).count();
    if (!echoed) {
        result.error = "movement ack timeout";
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

std::string FormatLatencyStatsJson(const Engine::Tests::LatencyStats& stats) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(3);
    json << "{"
         << "\"count\":" << stats.sampleCount << ","
         << "\"min_ms\":" << stats.minMs << ","
         << "\"mean_ms\":" << stats.meanMs << ","
         << "\"p50_ms\":" << stats.p50Ms << ","
         << "\"p95_ms\":" << stats.p95Ms << ","
         << "\"p99_ms\":" << stats.p99Ms << ","
         << "\"max_ms\":" << stats.maxMs << "}";
    return json.str();
}

void EmitJoinLoadProfile(const JoinLoadScenarioResult& result) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(3);
    json << "{"
         << "\"player_count_target\":" << result.playerCountTarget << ","
         << "\"player_count_joined\":" << result.successCount << ","
         << "\"player_count_movement_ok\":" << result.movementSuccessCount << ","
         << "\"burst_wall_ms\":" << result.burstWallMs << ","
         << "\"movement_burst_wall_ms\":" << result.movementBurstWallMs << ","
         << "\"connect\":" << FormatLatencyStatsJson(result.connectStats) << ","
         << "\"login\":" << FormatLatencyStatsJson(result.loginStats) << ","
         << "\"total_join\":" << FormatLatencyStatsJson(result.totalStats) << ","
         << "\"movement_rtt\":" << FormatLatencyStatsJson(result.movementStats) << ","
         << "\"server\":{"
         << "\"tick_ms\":" << result.tickMetrics.tickMs << ","
         << "\"tick_p95_ms\":" << result.tickMetrics.tickP95Ms << ","
         << "\"tick_max_ms\":" << result.tickMetrics.tickMsMax << ","
         << "\"connected_players\":" << result.tickMetrics.connectedPlayers << ","
         << "\"cell_density_max\":" << result.tickMetrics.cellDensityMax << ","
         << "\"layering_active\":" << (result.tickMetrics.layeringActive ? 1 : 0) << ","
         << "\"shard_redirect\":" << (result.tickMetrics.shardRedirectRecommended ? 1 : 0) << ","
         << "\"movement_cmds\":" << result.sessionMetrics.movementCommandsProcessed << ","
         << "\"movement_dropped\":" << result.sessionMetrics.movementCommandsDropped << ","
         << "\"peer_transforms_sent\":" << result.tickMetrics.peerTransformsSent << ","
         << "\"replication_queue_depth\":" << result.tickMetrics.replicationQueueDepth << ","
         << "\"dirty_snapshots\":" << result.sessionMetrics.dirtySnapshotsSent << ","
         << "\"visibility_events\":" << result.sessionMetrics.visibilityEvents << ","
         << "\"login_dropped\":" << result.loginDropped
         << "},"
         << "\"passed\":" << (result.passed ? "true" : "false")
         << "}";

    const std::string payload = json.str();
    std::cout << "[JoinLoadProfile] " << payload << std::endl;
}

TEST_F(AuthoritativeServerFixture, HundredPlayerConcurrentJoinLatency) {
    if (!JoinLoadEnabled()) {
        GTEST_SKIP() << "Set ENGINE_RUN_JOIN_LOAD_TEST=1 to run the 100-player join load test";
    }

    if (std::getenv("JOIN_LOAD_PLAYER_COUNT") == nullptr) {
        playerCount = kDefaultPlayerCount;
        joinRetryCount = JoinRetryCountForPlayers(playerCount);
    }

    const JoinLoadScenarioResult scenario = RunJoinLoadScenario();
    EXPECT_TRUE(scenario.passed) << "Join load scenario did not reach full success";
}

TEST_F(AuthoritativeServerFixture, ConcurrentJoinLoadStressProfile) {
    if (!JoinLoadEnabled() && !StressProfileEnabled()) {
        GTEST_SKIP() << "Set ENGINE_RUN_JOIN_LOAD_TEST=1 or JOIN_LOAD_STRESS_PROFILE=1 to run stress profile";
    }

    const JoinLoadScenarioResult scenario = RunJoinLoadScenario();
    if (!AllowPartialSuccess()) {
        EXPECT_TRUE(scenario.passed) << "Stress profile run did not reach full success";
    } else {
        EXPECT_GT(scenario.successCount, 0u) << "Stress profile run produced zero successful joins";
    }
}

} // namespace
