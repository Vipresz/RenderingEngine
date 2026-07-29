#pragma once

#include "core/ecs/Registry.h"
#include "core/network/CharacterMovementSimulator.h"
#include "core/network/LoadoutReplication.h"
#include "core/network/InventoryAction.h"
#include "core/network/ItemOwnershipReplication.h"
#include "core/network/ChatMessage.h"
#include "core/network/MovementCommand.h"
#include "core/network/TradeReplication.h"
#include "core/network/TradeSession.h"
#include "core/network/NetTypes.h"
#include "core/network/ReplicationTypes.h"
#include "core/terrain/HeightmapTerrain.h"
#include "network/NetServer.h"
#include "network/ServerMetrics.h"
#include "network/systems/ReplicationSystem.h"

#include <glm/glm.hpp>

#include <deque>
#include <functional>
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

    using ItemStackCapResolver = std::function<uint32_t(const std::string& itemId)>;
    void SetItemStackCapResolver(ItemStackCapResolver resolver) {
        m_stackCapResolver = std::move(resolver);
    }
    using InventoryActionValidator =
        std::function<bool(Engine::Net::InventoryActionType action, const std::string& itemId)>;
    void SetInventoryActionValidator(InventoryActionValidator validator) {
        m_inventoryActionValidator = std::move(validator);
    }
    void SetBagCapacity(uint16_t capacity) { m_bagCapacity = capacity; }

    // Thread-safe: enqueue only.
    void OnClientLoggedIn(Engine::Net::UserId userId,
                          const std::string& displayName,
                          const std::vector<std::pair<std::string, uint32_t>>& starterInventory = {},
                          const Engine::Net::ReplicatedItemOwnership* restoredOwnership = nullptr);
    void OnClientDisconnected(Engine::Net::UserId userId);
    bool HandleGenericData(Engine::Net::UserId userId, Engine::Net::GenericDataPayload& payload);
    bool HandleMovementCommand(Engine::Net::UserId userId, Engine::Net::MovementCommand& command);

    // Call from the server tick thread only.
    void Update(float deltaTime);

    void FlushDirtySnapshotForUser(Engine::Net::UserId userId);

    bool GrantOwnedItem(Engine::Net::UserId userId,
                        const std::string& itemId,
                        uint32_t quantity = 1,
                        uint64_t instanceId = 0);
    bool RemoveOwnedItem(Engine::Net::UserId userId, const std::string& itemId, uint32_t quantity = 1);
    bool RemoveOwnedItemFromSlot(Engine::Net::UserId userId,
                                 uint16_t slotIndex,
                                 uint32_t quantity = 1,
                                 bool removeEntireStack = false);
    bool RemoveOwnedItemInstance(Engine::Net::UserId userId, uint64_t instanceId);
    uint32_t CountOwnedItem(Engine::Net::UserId userId, const std::string& itemId) const;
    bool HasOwnedItem(Engine::Net::UserId userId, const std::string& itemId, uint32_t quantity = 1) const;
    const Engine::Net::ReplicatedItemOwnership* GetOwnedItems(Engine::Net::UserId userId) const;

    bool RequestTrade(Engine::Net::UserId fromUserId, Engine::Net::UserId toUserId);
    bool RespondToTrade(Engine::Net::UserId responderUserId,
                        Engine::Net::UserId requesterUserId,
                        bool accept);
    bool CancelTrade(Engine::Net::UserId userId);
    bool SetTradeOffer(Engine::Net::UserId userId, const std::vector<Engine::Net::TradeOfferItem>& offer);
    bool LockTradeOffer(Engine::Net::UserId userId);
    bool UnlockTradeOffer(Engine::Net::UserId userId);
    const Engine::Net::TradeSession* GetTradeSessionFor(Engine::Net::UserId userId) const;

    const ServerTickMetrics& GetLastTickMetrics() const { return m_lastTickMetrics; }
    const ServerTickMetrics& GetSessionMetrics() const { return m_sessionMetrics; }

private:
    struct PlayerState {
        EntityID entity = INVALID_ENTITY;
        Engine::Net::CharacterMovementState movement;
        std::string displayName;
        Engine::Net::ReplicatedLoadout loadout;
        Engine::Net::ReplicatedItemOwnership ownership;
        Engine::Net::ReplicatedPlayerTrade trade;
        uint64_t lastProcessedTick = 0;
        bool snapshotDirty = false;
        std::unordered_set<Engine::Net::UserId> visiblePeers;
    };

    struct PendingLogin {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        std::string displayName;
        std::vector<std::pair<std::string, uint32_t>> starterInventory;
        Engine::Net::ReplicatedItemOwnership restoredOwnership;
        bool hasRestoredOwnership = false;
    };

    struct PendingCommand {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::MovementCommand command;
    };

    struct PendingLoadout {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::ReplicatedLoadout loadout;
    };

    struct PendingOwnership {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::ReplicatedItemOwnership ownership;
    };

    struct PendingInventoryAction {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::InventoryActionCommand command;
    };

    struct PendingTradeCommand {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::TradeCommand command;
    };

    struct PendingChatCommand {
        Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
        Engine::Net::ChatCommand command;
    };

    void DrainQueues();
    void SpawnPlayer(Engine::Net::UserId userId, const std::string& displayName);
    void DespawnPlayer(Engine::Net::UserId userId);
    void ApplyMovement(Engine::Net::UserId userId, Engine::Net::MovementCommand command);
    bool ApplyMovementSimulation(Engine::Net::UserId userId, Engine::Net::MovementCommand& command);
    void ApplyLoadout(Engine::Net::UserId userId, Engine::Net::ReplicatedLoadout loadout);
    void ApplyOwnership(Engine::Net::UserId userId, Engine::Net::ReplicatedItemOwnership ownership);
    void ApplyClientBagLayout(Engine::Net::UserId userId, Engine::Net::ReplicatedItemOwnership ownership);
    void ApplyInventoryAction(Engine::Net::UserId userId, Engine::Net::InventoryActionCommand command);
    void RegisterLoadoutBinding();
    void RegisterOwnershipBinding();
    void RegisterTradeBinding();
    void ApplyTradeCommand(Engine::Net::UserId userId, Engine::Net::TradeCommand command);
    void ApplyChatMessage(Engine::Net::UserId userId, Engine::Net::ChatCommand command);
    void SyncTradeStateForSession(const Engine::Net::TradeSession& session);
    void EndTradeSession(uint64_t sessionId, ECS::TradePhase finalPhase);
    Engine::Net::TradeSession* FindTradeSessionFor(Engine::Net::UserId userId);
    const Engine::Net::TradeSession* FindTradeSessionFor(Engine::Net::UserId userId) const;
    bool TryExecuteTrade(Engine::Net::TradeSession& session);
    void MarkTradeParticipantsDirty(const Engine::Net::TradeSession& session);
    bool PlayerAvailableForTrade(Engine::Net::UserId userId) const;
    void BroadcastAuthoritativeTransform(Engine::Net::UserId userId,
                                         const PlayerState& player,
                                         uint64_t tick);
    void SendWorldSnapshotTo(Engine::Net::UserId userId, uint64_t tick);
    void UpdatePeerVisibility(Engine::Net::UserId observerId, const glm::vec3& observerPosition);
    void EnqueueDeferredVisibilityUpdate(Engine::Net::UserId observerId);
    void ProcessDeferredVisibilityUpdates();
    void NotifyInterestedPeersOfSpawn(Engine::Net::UserId userId,
                                      const glm::vec3& spawnPosition,
                                      const std::string& displayName,
                                      const Engine::Net::TransformStatePayload& spawnTransform);
    glm::vec3 ComputeSpawnPosition(size_t spawnIndex) const;
    void RefreshScalePolicy();
    static float ComputeP95(const std::deque<float>& samples);
    uint32_t ResolveStackCap(const std::string& itemId) const;
    bool ValidateBagOwnership(const Engine::Net::ReplicatedItemOwnership& ownership) const;
    bool OwnershipMultisetMatches(const Engine::Net::ReplicatedItemOwnership& current,
                                  const Engine::Net::ReplicatedItemOwnership& proposed) const;

    ItemStackCapResolver m_stackCapResolver;
    InventoryActionValidator m_inventoryActionValidator;
    uint16_t m_bagCapacity = Engine::ECS::kDefaultBagCapacity;
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
    uint32_t m_tickPeerTransformsSent = 0;
    size_t m_nextSpawnIndex = 0;
    std::deque<float> m_tickMsHistory;

    std::mutex m_queueMutex;
    std::deque<PendingLogin> m_pendingLogins;
    std::deque<Engine::Net::UserId> m_pendingDisconnects;
    std::deque<PendingCommand> m_pendingCommands;
    std::deque<PendingLoadout> m_pendingLoadouts;
    std::deque<PendingOwnership> m_pendingOwnership;
    std::deque<PendingInventoryAction> m_pendingInventoryActions;
    std::deque<PendingTradeCommand> m_pendingTradeCommands;
    std::deque<PendingChatCommand> m_pendingChatCommands;
    std::unordered_map<Engine::Net::UserId, double> m_lastChatSendSeconds;
    std::unordered_map<uint64_t, Engine::Net::TradeSession> m_tradeSessions;
    std::unordered_map<Engine::Net::UserId, uint64_t> m_userTradeSession;
    uint64_t m_nextTradeSessionId = 1;
    std::deque<Engine::Net::UserId> m_deferredVisibilityUpdates;
    std::unordered_set<Engine::Net::UserId> m_deferredVisibilityPending;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
