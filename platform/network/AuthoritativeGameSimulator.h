#pragma once

#include "core/ecs/Registry.h"
#include "core/network/CharacterMovementSimulator.h"
#include "core/network/LoadoutReplication.h"
#include "core/network/MovementCommand.h"
#include "core/network/NetTypes.h"
#include "core/network/ReplicationTypes.h"
#include "core/terrain/HeightmapTerrain.h"
#include "network/NetServer.h"
#include "network/ServerMetrics.h"
#include "network/systems/ReplicationSystem.h"

#include <glm/glm.hpp>

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Engine {
namespace Platform {
namespace Networking {

using TerrainHeightSampler = Engine::Net::TerrainHeightSampler;

// Headless authoritative simulation: applies movement commands and broadcasts replicated state.
// IO threads only enqueue events; Update() owns all ECS / player-map mutations.
class AuthoritativeGameSimulator {
public:
    explicit AuthoritativeGameSimulator(NetServer& server);

    void SetTerrainSampler(TerrainHeightSampler sampler) { m_terrainSampler = std::move(sampler); }
    bool LoadHeightmap(const std::string& imagePath, float sizeX, float sizeZ, float heightScale = 10.0f);
    void SetSpawnPosition(const glm::vec3& position) { m_spawnPosition = position; }
    void SetMovementSpeeds(float walkSpeed, float runSpeed) {
        m_movementSpeeds.walkSpeed = walkSpeed;
        m_movementSpeeds.runSpeed = runSpeed > walkSpeed ? runSpeed : walkSpeed;
    }
    const Engine::Net::MovementSpeedConfig& GetMovementSpeeds() const { return m_movementSpeeds; }

    // Thread-safe: enqueue only.
    void OnClientLoggedIn(Engine::Net::UserId userId, const std::string& displayName);
    void OnClientDisconnected(Engine::Net::UserId userId);
    bool HandleGenericData(Engine::Net::UserId userId, Engine::Net::GenericDataPayload& payload);
    bool HandleMovementCommand(Engine::Net::UserId userId, Engine::Net::MovementCommand& command);

    // Call from the server tick thread only.
    void Update(float deltaTime);

    const ServerTickMetrics& GetLastTickMetrics() const { return m_lastTickMetrics; }
    const ServerTickMetrics& GetSessionMetrics() const { return m_sessionMetrics; }

private:
    struct PlayerState {
        EntityID entity = INVALID_ENTITY;
        Engine::Net::CharacterMovementState movement;
        std::string displayName;
        Engine::Net::ReplicatedLoadout loadout;
        uint64_t lastProcessedTick = 0;
        bool snapshotDirty = false;
        std::unordered_set<Engine::Net::UserId> visiblePeers;
    };

    struct PendingLogin {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        std::string displayName;
    };

    struct PendingCommand {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::MovementCommand command;
    };

    struct PendingLoadout {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::ReplicatedLoadout loadout;
    };

    void DrainQueues();
    void SpawnPlayer(Engine::Net::UserId userId, const std::string& displayName);
    void DespawnPlayer(Engine::Net::UserId userId);
    void ApplyMovement(Engine::Net::UserId userId, Engine::Net::MovementCommand command);
    void ApplyLoadout(Engine::Net::UserId userId, Engine::Net::ReplicatedLoadout loadout);
    void RegisterLoadoutBinding();
    void BroadcastAuthoritativeTransform(Engine::Net::UserId userId,
                                         const PlayerState& player,
                                         uint64_t tick);
    void SendWorldSnapshotTo(Engine::Net::UserId userId, uint64_t tick);
    void UpdatePeerVisibility(Engine::Net::UserId observerId, const glm::vec3& observerPosition);
    void NotifyInterestedPeersOfSpawn(Engine::Net::UserId userId,
                                      const glm::vec3& spawnPosition,
                                      const std::string& displayName,
                                      const Engine::Net::TransformStatePayload& spawnTransform);
    glm::vec3 ComputeSpawnPosition(size_t spawnIndex) const;
    Engine::Net::TransformStatePayload BuildTransformPayload(Engine::Net::UserId userId,
                                                             const PlayerState& player,
                                                             uint64_t tick) const;

    NetServer& m_server;
    ECS::Registry m_registry;
    Systems::ReplicationSystem m_replication;
    std::unordered_map<Engine::Net::UserId, PlayerState> m_players;
    std::unordered_map<EntityID, Engine::Net::UserId> m_entityToUser;
    std::unique_ptr<Engine::Terrain::HeightmapTerrain> m_heightmap;
    TerrainHeightSampler m_terrainSampler;
    glm::vec3 m_spawnPosition{5.0f, 1.0f, 5.0f};
    Engine::Net::MovementSpeedConfig m_movementSpeeds;
    float m_snapshotAccumulator = 0.0f;
    float m_snapshotIntervalSeconds = 0.1f;
    uint64_t m_tickNumber = 0;
    ServerTickMetrics m_lastTickMetrics;
    ServerTickMetrics m_sessionMetrics;
    uint32_t m_tickMovementCommandsProcessed = 0;
    uint32_t m_tickDirtySnapshotsSent = 0;
    uint32_t m_tickVisibilityEvents = 0;
    size_t m_nextSpawnIndex = 0;

    std::mutex m_queueMutex;
    std::deque<PendingLogin> m_pendingLogins;
    std::deque<Engine::Net::UserId> m_pendingDisconnects;
    std::deque<PendingCommand> m_pendingCommands;
    std::deque<PendingLoadout> m_pendingLoadouts;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
