#include "network/AuthoritativeGameSimulator.h"

#include "Logger.h"
#include "core/network/CharacterMovementSimulator.h"
#include "render/components/AnimationComponent.h"
#include "render/components/TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

using Engine::Platform::Render::Components::AnimationComponent;
using Engine::Platform::Render::Components::TransformComponent;

namespace Engine {
namespace Platform {
namespace Networking {

AuthoritativeGameSimulator::AuthoritativeGameSimulator(NetServer& server) : m_server(server) {
    RegisterLoadoutBinding();
    m_replication.SetSnapshotSendHandler([this](const Engine::Net::EntitySnapshot& snapshot) {
        Engine::Net::NetPacket packet = Engine::Net::MakeMessage(
            Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), 0);
        auto playerIt = m_players.find(snapshot.entityId);
        if (playerIt != m_players.end()) {
            m_server.BroadcastPacketToInterested(
                packet, snapshot.entityId, playerIt->second.movement.position);
            return;
        }
        m_server.BroadcastPacket(packet);
    });
}

void AuthoritativeGameSimulator::RegisterLoadoutBinding() {
    Engine::Net::ReplicationComponentDescriptor loadoutDesc;
    loadoutDesc.id = Engine::Net::kLoadoutReplicationComponent;
    loadoutDesc.name = "Loadout";
    m_replication.GetRegistry().Register(std::move(loadoutDesc));

    m_replication.RegisterComponentBinding(Systems::ReplicationComponentBinding{
        Engine::Net::kLoadoutReplicationComponent,
        [this](EntityID entity, ECS::Registry& /*registry*/, Engine::Net::NetBuffer& out) {
            const auto userIt = m_entityToUser.find(entity);
            if (userIt == m_entityToUser.end()) {
                return false;
            }
            const auto playerIt = m_players.find(userIt->second);
            if (playerIt == m_players.end() || playerIt->second.loadout.bodyItemId.empty()) {
                return false;
            }
            out = playerIt->second.loadout.Serialize();
            return true;
        },
        [](EntityID /*entity*/, ECS::Registry& /*registry*/, const Engine::Net::NetBuffer& /*in*/) {
            // Server is the publisher; clients apply loadout via their own bindings.
            return true;
        }});
}

bool AuthoritativeGameSimulator::LoadHeightmap(const std::string& imagePath,
                                               float sizeX,
                                               float sizeZ,
                                               float heightScale) {
    m_heightmap = std::make_unique<Engine::Terrain::HeightmapTerrain>();
    if (!m_heightmap->LoadFromImage(imagePath, sizeX, sizeZ, heightScale)) {
        m_heightmap.reset();
        return false;
    }

    Engine::Terrain::HeightmapTerrain* terrain = m_heightmap.get();
    m_terrainSampler = [terrain](float x, float z) { return terrain->GetHeightAt(x, z); };

    const float spawnHeight = terrain->GetHeightAt(m_spawnPosition.x, m_spawnPosition.z);
    m_spawnPosition.y = spawnHeight;
    return true;
}

void AuthoritativeGameSimulator::OnClientLoggedIn(Engine::Net::UserId userId,
                                                  const std::string& displayName) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingLogins.push_back(PendingLogin{userId, displayName});
}

void AuthoritativeGameSimulator::OnClientDisconnected(Engine::Net::UserId userId) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingDisconnects.push_back(userId);
}

bool AuthoritativeGameSimulator::HandleGenericData(Engine::Net::UserId userId,
                                                   Engine::Net::GenericDataPayload& payload) {
    if (payload.channel == Engine::Net::kMovementCommandChannel) {
        Engine::Net::MovementCommand command;
        if (!Engine::Net::DeserializeMovementCommand(Engine::Net::NetBuffer(payload.data), command)) {
            return true;
        }
        return HandleMovementCommand(userId, command);
    }

    if (payload.channel == Engine::Net::kLoadoutChannel) {
        Engine::Net::ReplicatedLoadout loadout;
        if (!Engine::Net::ReplicatedLoadout::Deserialize(Engine::Net::NetBuffer(payload.data), loadout)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingLoadouts.push_back(PendingLoadout{userId, std::move(loadout)});
        return true;
    }

    return false;
}

bool AuthoritativeGameSimulator::HandleMovementCommand(Engine::Net::UserId userId,
                                                     Engine::Net::MovementCommand& command) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_pendingCommands.push_back(PendingCommand{userId, command});
    return true;
}

void AuthoritativeGameSimulator::DrainQueues() {
    std::deque<PendingLogin> logins;
    std::deque<Engine::Net::UserId> disconnects;
    std::deque<PendingCommand> commands;
    std::deque<PendingLoadout> loadouts;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        logins.swap(m_pendingLogins);
        disconnects.swap(m_pendingDisconnects);
        commands.swap(m_pendingCommands);
        loadouts.swap(m_pendingLoadouts);
    }

    for (const PendingLogin& login : logins) {
        SpawnPlayer(login.userId, login.displayName);
    }
    for (Engine::Net::UserId userId : disconnects) {
        DespawnPlayer(userId);
    }
    for (PendingLoadout& entry : loadouts) {
        ApplyLoadout(entry.userId, std::move(entry.loadout));
    }

    std::vector<Engine::Net::TransformStatePayload> peerEnqueuePayloads;
    peerEnqueuePayloads.reserve(commands.size());

    for (PendingCommand& entry : commands) {
        if (!ApplyMovementSimulation(entry.userId, entry.command)) {
            continue;
        }
        ++m_tickMovementCommandsProcessed;

        const auto playerIt = m_players.find(entry.userId);
        if (playerIt == m_players.end()) {
            continue;
        }

        Engine::Net::TransformStatePayload payload =
            BuildTransformPayload(entry.userId, playerIt->second, entry.command.tick);
        m_server.SendMovementAck(entry.userId, entry.command.tick);
        m_server.StoreAuthoritativeTransform(entry.userId, payload);
        m_server.EnqueueSelfTransform(entry.userId, payload);
        peerEnqueuePayloads.push_back(payload);
        EnqueueDeferredVisibilityUpdate(entry.userId);
    }

    for (const Engine::Net::TransformStatePayload& payload : peerEnqueuePayloads) {
        m_server.EnqueueTransformToPeers(payload, payload.userId, Engine::Net::kInvalidUserId);
    }
}

void AuthoritativeGameSimulator::SpawnPlayer(Engine::Net::UserId userId, const std::string& displayName) {
    if (m_players.find(userId) != m_players.end()) {
        return;
    }

    const glm::vec3 spawnPosition = ComputeSpawnPosition(m_nextSpawnIndex++);

    const ECS::Entity entity = m_registry.CreateEntity();
    auto& transform = m_registry.AddComponent<TransformComponent>(entity.id);
    transform.SetPosition(spawnPosition.x, spawnPosition.y, spawnPosition.z);
    transform.SetScale(1.0f);
    m_registry.AddComponent<AnimationComponent>(entity.id);

    PlayerState player;
    player.entity = entity.id;
    player.displayName = displayName;
    player.movement.position = spawnPosition;
    player.movement.groundLevel = spawnPosition.y;
    if (m_terrainSampler) {
        player.movement.groundLevel = m_terrainSampler(spawnPosition.x, spawnPosition.z);
        player.movement.position.y = player.movement.groundLevel;
        transform.SetPosition(player.movement.position.x, player.movement.position.y,
                              player.movement.position.z);
    }

    m_replication.RegisterReplicatedEntity(userId, entity.id);
    m_entityToUser[entity.id] = userId;
    player.snapshotDirty = true;

    const std::vector<Engine::Net::UserId> nearbyUsers =
        m_server.GetInterestManager().QueryNearbyUsers(
            player.movement.position, m_server.GetInterestManager().GetDefaultInterestRadius());
    for (Engine::Net::UserId peerId : nearbyUsers) {
        if (peerId != userId) {
            player.visiblePeers.insert(peerId);
        }
    }

    m_players[userId] = player;

    const Engine::Net::TransformStatePayload spawnTransform =
        BuildTransformPayload(userId, player, m_tickNumber);
    m_server.StoreAuthoritativeTransform(userId, spawnTransform);
    m_server.SendInterestFilteredJoinState(userId, player.movement.position);
    NotifyInterestedPeersOfSpawn(userId, player.movement.position, displayName, spawnTransform);
    SendWorldSnapshotTo(userId, m_tickNumber);

    LOG_INFO_FMT("AuthoritativeGameSimulator", "Spawned player %u (%s)", userId, displayName.c_str());
}

void AuthoritativeGameSimulator::DespawnPlayer(Engine::Net::UserId userId) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return;
    }

    m_entityToUser.erase(it->second.entity);
    m_replication.UnregisterReplicatedEntity(userId);
    m_registry.DestroyEntity(it->second.entity);
    m_players.erase(it);
}

void AuthoritativeGameSimulator::ApplyLoadout(Engine::Net::UserId userId,
                                              Engine::Net::ReplicatedLoadout loadout) {
    auto it = m_players.find(userId);
    if (it == m_players.end()) {
        return;
    }
    if (loadout.bodyItemId.empty()) {
        return;
    }
    // Basic size guard against abusive equipment lists.
    if (loadout.equipment.size() > 32) {
        loadout.equipment.resize(32);
    }
    it->second.loadout = std::move(loadout);
    it->second.snapshotDirty = true;
}

void AuthoritativeGameSimulator::ApplyMovement(Engine::Net::UserId userId,
                                               Engine::Net::MovementCommand command) {
    if (!ApplyMovementSimulation(userId, command)) {
        return;
    }
    auto playerIt = m_players.find(userId);
    if (playerIt == m_players.end()) {
        return;
    }
    BroadcastAuthoritativeTransform(userId, playerIt->second, command.tick);
    UpdatePeerVisibility(userId, playerIt->second.movement.position);
}

bool AuthoritativeGameSimulator::ApplyMovementSimulation(Engine::Net::UserId userId,
                                                         Engine::Net::MovementCommand& command) {
    auto playerIt = m_players.find(userId);
    if (playerIt == m_players.end()) {
        return false;
    }

    if (command.tick <= playerIt->second.lastProcessedTick) {
        return false;
    }

    Engine::Net::SanitizeMovementCommand(command, m_movementSpeeds);
    playerIt->second.lastProcessedTick = command.tick;

    PlayerState& player = playerIt->second;
    Engine::Net::CharacterMovementSimulator::SimulateStep(
        player.movement, command, m_terrainSampler);

    if (TransformComponent* transform = m_registry.GetComponent<TransformComponent>(player.entity)) {
        transform->position = player.movement.position;
        transform->SetYaw(player.movement.yawDegrees);
    }

    if (AnimationComponent* anim = m_registry.GetComponent<AnimationComponent>(player.entity)) {
        anim->isJumping = player.movement.isJumping;
        switch (player.movement.animationState) {
            case Engine::Net::MovementAnimationState::Walking:
                anim->currentState = AnimationComponent::AnimationState::Walking;
                break;
            case Engine::Net::MovementAnimationState::Running:
                anim->currentState = AnimationComponent::AnimationState::Running;
                break;
            case Engine::Net::MovementAnimationState::Jumping:
                anim->currentState = AnimationComponent::AnimationState::Jumping;
                break;
            case Engine::Net::MovementAnimationState::TurningLeft:
                anim->currentState = AnimationComponent::AnimationState::TurningLeft;
                break;
            case Engine::Net::MovementAnimationState::TurningRight:
                anim->currentState = AnimationComponent::AnimationState::TurningRight;
                break;
            case Engine::Net::MovementAnimationState::Idle:
            default:
                anim->currentState = AnimationComponent::AnimationState::Idle;
                break;
        }
        anim->isRunning = anim->currentState == AnimationComponent::AnimationState::Running;
        anim->airVelocityX = player.movement.airVelocityX;
        anim->airVelocityZ = player.movement.airVelocityZ;
        anim->locomotionSpeed = std::sqrt(
            player.movement.airVelocityX * player.movement.airVelocityX +
            player.movement.airVelocityZ * player.movement.airVelocityZ);
    }

    return true;
}

void AuthoritativeGameSimulator::Update(float deltaTime) {
    const auto tickStart = std::chrono::steady_clock::now();
    m_tickMovementCommandsProcessed = 0;
    m_tickDirtySnapshotsSent = 0;
    m_tickVisibilityEvents = 0;
    m_tickPeerTransformsSent = 0;

    DrainQueues();
    ProcessDeferredVisibilityUpdates();

    m_snapshotAccumulator += deltaTime;
    if (m_snapshotAccumulator >= m_snapshotIntervalSeconds) {
        m_snapshotAccumulator = 0.0f;
        ++m_tickNumber;

        for (auto& entry : m_players) {
            if (!entry.second.snapshotDirty) {
                continue;
            }
            const Engine::Net::EntitySnapshot snapshot =
                m_replication.BuildEntitySnapshot(entry.first, m_registry, m_tickNumber);
            if (snapshot.components.empty()) {
                entry.second.snapshotDirty = false;
                continue;
            }
            Engine::Net::NetPacket packet = Engine::Net::MakeMessage(
                Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), 0);
            m_server.BroadcastPacketToInterested(
                packet, entry.first, entry.second.movement.position);
            entry.second.snapshotDirty = false;
            ++m_tickDirtySnapshotsSent;
        }
    }

    const size_t replicationBudget = m_server.GetScalePolicyConfig().maxPeerTransformsPerTick;
    m_tickPeerTransformsSent =
        static_cast<uint32_t>(m_server.FlushPendingPeerTransforms(replicationBudget));

    m_lastTickMetrics.tickMs =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - tickStart).count();
    m_lastTickMetrics.tickMsMax =
        (std::max)(m_lastTickMetrics.tickMsMax, m_lastTickMetrics.tickMs);
    m_sessionMetrics.sessionTickMsMax =
        (std::max)(m_sessionMetrics.sessionTickMsMax, m_lastTickMetrics.tickMs);
    m_lastTickMetrics.connectedPlayers = static_cast<uint32_t>(m_players.size());
    m_lastTickMetrics.movementCommandsProcessed = m_tickMovementCommandsProcessed;
    m_lastTickMetrics.dirtySnapshotsSent = m_tickDirtySnapshotsSent;
    m_lastTickMetrics.visibilityEvents = m_tickVisibilityEvents;
    m_lastTickMetrics.movementCommandsDropped = m_server.ConsumeMovementCommandsDropped();
    m_lastTickMetrics.peerTransformsSent = m_tickPeerTransformsSent;
    m_lastTickMetrics.replicationQueueDepth =
        static_cast<uint32_t>(m_server.GetReplicationQueueDepth());
    m_lastTickMetrics.peerTransformsDeferred = m_lastTickMetrics.replicationQueueDepth;
    m_lastTickMetrics.tickNumber = m_tickNumber;

    RefreshScalePolicy();

    m_sessionMetrics.connectedPlayers = m_lastTickMetrics.connectedPlayers;
    m_sessionMetrics.movementCommandsProcessed += m_tickMovementCommandsProcessed;
    m_sessionMetrics.dirtySnapshotsSent += m_tickDirtySnapshotsSent;
    m_sessionMetrics.visibilityEvents += m_tickVisibilityEvents;
    m_sessionMetrics.movementCommandsDropped += m_lastTickMetrics.movementCommandsDropped;
    m_sessionMetrics.sessionPeerTransformsSent += m_tickPeerTransformsSent;
    m_sessionMetrics.sessionPeerTransformsDeferred = m_lastTickMetrics.replicationQueueDepth;
    m_sessionMetrics.tickNumber = m_tickNumber;
}

glm::vec3 AuthoritativeGameSimulator::ComputeSpawnPosition(size_t spawnIndex) const {
    const Engine::Net::ScalePolicyConfig& policy = m_server.GetScalePolicyConfig();
    const float cellSize = m_server.GetInterestManager().GetCellSize();
    const uint32_t densityTarget = std::max(policy.targetMaxCellDensity, 1u);
    const float spacing = (std::clamp)(cellSize / std::sqrt(static_cast<float>(densityTarget)),
                                     policy.spawnSpacingMin,
                                     policy.spawnSpacingMax);

    const size_t populationHint = (std::max)(m_players.size(), spawnIndex + 1);
    const int gridWidth =
        (std::max)(10, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(populationHint)))));

    const int row = static_cast<int>(spawnIndex / static_cast<size_t>(gridWidth));
    const int col = static_cast<int>(spawnIndex % static_cast<size_t>(gridWidth));
    glm::vec3 position = m_spawnPosition;
    position.x += static_cast<float>(col) * spacing;
    position.z += static_cast<float>(row) * spacing;
    if (m_terrainSampler) {
        position.y = m_terrainSampler(position.x, position.z);
    }
    return position;
}

float AuthoritativeGameSimulator::ComputeP95(const std::deque<float>& samples) {
    if (samples.empty()) {
        return 0.0f;
    }
    std::vector<float> sorted(samples.begin(), samples.end());
    std::sort(sorted.begin(), sorted.end());
    const size_t index =
        std::min(sorted.size() - 1, static_cast<size_t>(std::ceil(sorted.size() * 0.95) - 1));
    return sorted[index];
}

void AuthoritativeGameSimulator::RefreshScalePolicy() {
    const Engine::Net::ScalePolicyConfig& policy = m_server.GetScalePolicyConfig();
    const uint32_t cellDensity = m_server.GetInterestManager().GetMaxCellDensity();
    const bool layeringActive = policy.IsLayeringActive(cellDensity);
    m_server.ApplyLayeringState(layeringActive);

    m_tickMsHistory.push_back(m_lastTickMetrics.tickMs);
    constexpr size_t kTickHistorySize = 60;
    while (m_tickMsHistory.size() > kTickHistorySize) {
        m_tickMsHistory.pop_front();
    }

    const float tickP95Ms = ComputeP95(m_tickMsHistory);
    m_server.SetObservedTickP95Ms(tickP95Ms);

    float snapshotInterval = policy.snapshotIntervalNormalSeconds;
    if (layeringActive) {
        snapshotInterval = policy.snapshotIntervalCrowdedSeconds;
    }
    if (tickP95Ms >= policy.shardTickMsSoftLimit) {
        snapshotInterval = std::max(snapshotInterval, policy.snapshotIntervalStressedSeconds);
    }
    m_snapshotIntervalSeconds = snapshotInterval;

    const uint32_t players = static_cast<uint32_t>(m_players.size());
    m_lastTickMetrics.tickP95Ms = tickP95Ms;
    m_lastTickMetrics.cellDensityMax = cellDensity;
    m_lastTickMetrics.layeringActive = layeringActive;
    m_lastTickMetrics.shardRedirectRecommended = policy.ShouldRedirectLogin(players, tickP95Ms);
}

void AuthoritativeGameSimulator::NotifyInterestedPeersOfSpawn(
    Engine::Net::UserId userId,
    const glm::vec3& spawnPosition,
    const std::string& displayName,
    const Engine::Net::TransformStatePayload& spawnTransform) {
    const std::vector<Engine::Net::UserId> interestedObservers =
        m_server.GetInterestManager().GetInterestedObservers(userId, spawnPosition);
    const size_t visibilityBudget =
        m_server.GetScalePolicyConfig().maxVisibilityEventsPerTick;

    Engine::Net::PeerEventPayload joined;
    joined.userId = userId;
    joined.displayName = displayName;
    const Engine::Net::NetPacket joinedPacket =
        Engine::Net::MakeMessage(Engine::Net::MessageType::PeerJoined, joined.Serialize(), 0);

    for (Engine::Net::UserId observerId : interestedObservers) {
        if (observerId == userId) {
            continue;
        }
        if (m_tickVisibilityEvents >= visibilityBudget) {
            break;
        }

        auto observerIt = m_players.find(observerId);
        if (observerIt == m_players.end()) {
            continue;
        }

        if (observerIt->second.visiblePeers.insert(userId).second) {
            m_server.SendPacketToClient(observerId, joinedPacket);
            ++m_tickVisibilityEvents;
        }
    }

    m_server.SendTransformToInterested(spawnTransform, userId, Engine::Net::kInvalidUserId);
}

void AuthoritativeGameSimulator::BroadcastAuthoritativeTransform(Engine::Net::UserId userId,
                                                                 const PlayerState& player,
                                                                 uint64_t tick) {
    const Engine::Net::TransformStatePayload payload = BuildTransformPayload(userId, player, tick);
    m_server.StoreAuthoritativeTransform(userId, payload);
    m_server.SendTransformToInterested(payload, userId, Engine::Net::kInvalidUserId);
}

void AuthoritativeGameSimulator::SendWorldSnapshotTo(Engine::Net::UserId userId, uint64_t tick) {
    glm::vec3 observerPosition = m_spawnPosition;
    const auto playerIt = m_players.find(userId);
    if (playerIt != m_players.end()) {
        observerPosition = playerIt->second.movement.position;
    }

    std::vector<Engine::Net::UserId> nearbyUsers =
        m_server.GetInterestManager().QueryNearbyUsers(
            observerPosition, m_server.GetInterestManager().GetDefaultInterestRadius());

    Engine::Net::WorldSnapshotPayload world;
    world.tick = tick;
    world.entities.reserve(nearbyUsers.size());
    for (Engine::Net::UserId entityUserId : nearbyUsers) {
        const auto entryIt = m_players.find(entityUserId);
        if (entryIt == m_players.end()) {
            continue;
        }
        world.entities.push_back(
            m_replication.BuildEntitySnapshot(entityUserId, m_registry, tick));
    }
    m_server.SendWorldSnapshotTo(userId, world);
}

void AuthoritativeGameSimulator::EnqueueDeferredVisibilityUpdate(Engine::Net::UserId observerId) {
    if (m_deferredVisibilityPending.insert(observerId).second) {
        m_deferredVisibilityUpdates.push_back(observerId);
    }
}

void AuthoritativeGameSimulator::ProcessDeferredVisibilityUpdates() {
    const size_t visibilityBudget = m_server.GetScalePolicyConfig().maxVisibilityEventsPerTick;
    while (!m_deferredVisibilityUpdates.empty()) {
        if (m_tickVisibilityEvents >= visibilityBudget) {
            break;
        }

        const Engine::Net::UserId observerId = m_deferredVisibilityUpdates.front();
        m_deferredVisibilityUpdates.pop_front();
        m_deferredVisibilityPending.erase(observerId);

        const auto playerIt = m_players.find(observerId);
        if (playerIt == m_players.end()) {
            continue;
        }
        UpdatePeerVisibility(observerId, playerIt->second.movement.position);
    }
}

void AuthoritativeGameSimulator::UpdatePeerVisibility(Engine::Net::UserId observerId,
                                                      const glm::vec3& observerPosition) {
    auto observerIt = m_players.find(observerId);
    if (observerIt == m_players.end()) {
        return;
    }

    const std::vector<Engine::Net::UserId> nearbyUsers =
        m_server.GetInterestManager().QueryNearbyUsers(
            observerPosition, m_server.GetInterestManager().GetDefaultInterestRadius());

    std::unordered_set<Engine::Net::UserId> nextVisible;
    nextVisible.reserve(nearbyUsers.size());
    for (Engine::Net::UserId peerId : nearbyUsers) {
        if (peerId != observerId) {
            nextVisible.insert(peerId);
        }
    }

    PlayerState& observer = observerIt->second;
    for (Engine::Net::UserId peerId : nextVisible) {
        if (observer.visiblePeers.find(peerId) != observer.visiblePeers.end()) {
            continue;
        }
        const auto peerIt = m_players.find(peerId);
        if (peerIt == m_players.end()) {
            continue;
        }

        Engine::Net::PeerEventPayload joined;
        joined.userId = peerId;
        joined.displayName = peerIt->second.displayName;
        m_server.SendPacketToClient(
            observerId,
            Engine::Net::MakeMessage(Engine::Net::MessageType::PeerJoined, joined.Serialize(), 0));

        const Engine::Net::TransformStatePayload transform =
            BuildTransformPayload(peerId, peerIt->second, m_tickNumber);
        if (!m_server.HasUdpEndpointFor(observerId)) {
            m_server.SendPacketToClient(
                observerId,
                Engine::Net::MakeMessage(Engine::Net::MessageType::TransformState, transform.Serialize(), 0));
        }
        ++m_tickVisibilityEvents;
    }

    for (Engine::Net::UserId peerId : observer.visiblePeers) {
        if (nextVisible.find(peerId) != nextVisible.end()) {
            continue;
        }
        Engine::Net::PeerEventPayload left;
        left.userId = peerId;
        const auto peerIt = m_players.find(peerId);
        if (peerIt != m_players.end()) {
            left.displayName = peerIt->second.displayName;
        }
        m_server.SendPacketToClient(
            observerId,
            Engine::Net::MakeMessage(Engine::Net::MessageType::PeerLeft, left.Serialize(), 0));
        ++m_tickVisibilityEvents;
    }

    observer.visiblePeers = std::move(nextVisible);
}

Engine::Net::TransformStatePayload AuthoritativeGameSimulator::BuildTransformPayload(
    Engine::Net::UserId userId,
    const PlayerState& player,
    uint64_t tick) const {
    Engine::Net::TransformStatePayload payload;
    payload.userId = userId;
    payload.tick = tick;
    payload.position = player.movement.position;
    payload.rotationDegrees = glm::vec3(0.0f, player.movement.yawDegrees, 0.0f);
    payload.isJumping = player.movement.isJumping;
    payload.jumpAnimationActive = player.movement.jumpAnimationActive;
    payload.jumpVelocity = player.movement.jumpVelocity;
    payload.airVelocityX = player.movement.airVelocityX;
    payload.airVelocityZ = player.movement.airVelocityZ;
    payload.groundLevel = player.movement.groundLevel;
    payload.animationState = static_cast<uint8_t>(player.movement.animationState);
    return payload;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
