#include "network/AuthoritativeGameSimulator.h"

#include "Logger.h"
#include "core/network/CharacterMovementSimulator.h"
#include "render/components/AnimationComponent.h"
#include "render/components/TransformComponent.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
    for (PendingCommand& entry : commands) {
        ++m_tickMovementCommandsProcessed;
        ApplyMovement(entry.userId, entry.command);
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
    auto playerIt = m_players.find(userId);
    if (playerIt == m_players.end()) {
        return;
    }

    if (command.tick <= playerIt->second.lastProcessedTick) {
        return;
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

    BroadcastAuthoritativeTransform(userId, player, command.tick);
    UpdatePeerVisibility(userId, player.movement.position);
}

void AuthoritativeGameSimulator::Update(float deltaTime) {
    const auto tickStart = std::chrono::steady_clock::now();
    m_tickMovementCommandsProcessed = 0;
    m_tickDirtySnapshotsSent = 0;
    m_tickVisibilityEvents = 0;

    DrainQueues();

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

    m_lastTickMetrics.tickMs =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - tickStart).count();
    m_lastTickMetrics.connectedPlayers = static_cast<uint32_t>(m_players.size());
    m_lastTickMetrics.movementCommandsProcessed = m_tickMovementCommandsProcessed;
    m_lastTickMetrics.dirtySnapshotsSent = m_tickDirtySnapshotsSent;
    m_lastTickMetrics.visibilityEvents = m_tickVisibilityEvents;
    m_lastTickMetrics.movementCommandsDropped = m_server.ConsumeMovementCommandsDropped();
    m_lastTickMetrics.tickNumber = m_tickNumber;

    m_sessionMetrics.connectedPlayers = m_lastTickMetrics.connectedPlayers;
    m_sessionMetrics.movementCommandsProcessed += m_tickMovementCommandsProcessed;
    m_sessionMetrics.dirtySnapshotsSent += m_tickDirtySnapshotsSent;
    m_sessionMetrics.visibilityEvents += m_tickVisibilityEvents;
    m_sessionMetrics.movementCommandsDropped += m_lastTickMetrics.movementCommandsDropped;
    m_sessionMetrics.tickNumber = m_tickNumber;
}

glm::vec3 AuthoritativeGameSimulator::ComputeSpawnPosition(size_t spawnIndex) const {
    constexpr float kSpacing = 2.5f;
    constexpr int kGridWidth = 10;
    const int row = static_cast<int>(spawnIndex / kGridWidth);
    const int col = static_cast<int>(spawnIndex % kGridWidth);
    glm::vec3 position = m_spawnPosition;
    position.x += static_cast<float>(col) * kSpacing;
    position.z += static_cast<float>(row) * kSpacing;
    if (m_terrainSampler) {
        position.y = m_terrainSampler(position.x, position.z);
    }
    return position;
}

void AuthoritativeGameSimulator::NotifyInterestedPeersOfSpawn(
    Engine::Net::UserId userId,
    const glm::vec3& spawnPosition,
    const std::string& displayName,
    const Engine::Net::TransformStatePayload& spawnTransform) {
    const std::vector<Engine::Net::UserId> nearbyUsers =
        m_server.GetInterestManager().QueryNearbyUsers(
            spawnPosition, m_server.GetInterestManager().GetDefaultInterestRadius());

    Engine::Net::PeerEventPayload joined;
    joined.userId = userId;
    joined.displayName = displayName;
    const Engine::Net::NetPacket joinedPacket =
        Engine::Net::MakeMessage(Engine::Net::MessageType::PeerJoined, joined.Serialize(), 0);

    for (Engine::Net::UserId observerId : nearbyUsers) {
        if (observerId == userId) {
            continue;
        }

        auto observerIt = m_players.find(observerId);
        if (observerIt == m_players.end()) {
            continue;
        }

        if (observerIt->second.visiblePeers.insert(userId).second) {
            m_server.SendPacketToClient(observerId, joinedPacket);
            if (!m_server.HasUdpEndpointFor(observerId)) {
                m_server.SendPacketToClient(
                    observerId,
                    Engine::Net::MakeMessage(
                        Engine::Net::MessageType::TransformState, spawnTransform.Serialize(), 0));
            }
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
