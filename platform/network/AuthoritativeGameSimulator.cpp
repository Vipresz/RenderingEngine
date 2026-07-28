#include "network/AuthoritativeGameSimulator.h"

#include "Logger.h"
#include "core/network/CharacterMovementSimulator.h"
#include "render/components/AnimationComponent.h"
#include "render/components/TransformComponent.h"

#include <algorithm>
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
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingCommands.push_back(PendingCommand{userId, command});
        return true;
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
        ApplyMovement(entry.userId, entry.command);
    }
}

void AuthoritativeGameSimulator::SpawnPlayer(Engine::Net::UserId userId, const std::string& displayName) {
    if (m_players.find(userId) != m_players.end()) {
        return;
    }

    const ECS::Entity entity = m_registry.CreateEntity();
    auto& transform = m_registry.AddComponent<TransformComponent>(entity.id);
    transform.SetPosition(m_spawnPosition.x, m_spawnPosition.y, m_spawnPosition.z);
    transform.SetScale(1.0f);
    m_registry.AddComponent<AnimationComponent>(entity.id);

    PlayerState player;
    player.entity = entity.id;
    player.displayName = displayName;
    player.movement.position = m_spawnPosition;
    player.movement.groundLevel = m_spawnPosition.y;
    if (m_terrainSampler) {
        player.movement.groundLevel = m_terrainSampler(m_spawnPosition.x, m_spawnPosition.z);
        player.movement.position.y = player.movement.groundLevel;
        transform.SetPosition(player.movement.position.x, player.movement.position.y,
                              player.movement.position.z);
    }

    m_replication.RegisterReplicatedEntity(userId, entity.id);
    m_entityToUser[entity.id] = userId;
    m_players[userId] = player;

    const Engine::Net::TransformStatePayload spawnTransform =
        BuildTransformPayload(userId, player, m_tickNumber);
    m_server.StoreAuthoritativeTransform(userId, spawnTransform);
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
}

void AuthoritativeGameSimulator::Update(float deltaTime) {
    DrainQueues();

    m_snapshotAccumulator += deltaTime;
    if (m_snapshotAccumulator < m_snapshotIntervalSeconds) {
        return;
    }
    m_snapshotAccumulator = 0.0f;
    ++m_tickNumber;

    for (const auto& entry : m_players) {
        const Engine::Net::EntitySnapshot snapshot =
            m_replication.BuildEntitySnapshot(entry.first, m_registry, m_tickNumber);
        if (snapshot.components.empty()) {
            continue;
        }
        Engine::Net::NetPacket packet = Engine::Net::MakeMessage(
            Engine::Net::MessageType::EntitySnapshot, snapshot.Serialize(), 0);
        m_server.BroadcastPacketToInterested(
            packet, entry.first, entry.second.movement.position);
    }
}

void AuthoritativeGameSimulator::BroadcastAuthoritativeTransform(Engine::Net::UserId userId,
                                                                 const PlayerState& player,
                                                                 uint64_t tick) {
    const Engine::Net::TransformStatePayload payload = BuildTransformPayload(userId, player, tick);
    m_server.StoreAuthoritativeTransform(userId, payload);
    const Engine::Net::NetPacket packet = Engine::Net::MakeMessage(
        Engine::Net::MessageType::TransformState, payload.Serialize(), 0);
    m_server.BroadcastPacketToInterested(packet, userId, payload.position, userId);
    m_server.SendPacketToClient(userId, packet);
}

void AuthoritativeGameSimulator::SendWorldSnapshotTo(Engine::Net::UserId userId, uint64_t tick) {
    Engine::Net::WorldSnapshotPayload world;
    world.tick = tick;
    world.entities.reserve(m_players.size());
    for (const auto& entry : m_players) {
        world.entities.push_back(
            m_replication.BuildEntitySnapshot(entry.first, m_registry, tick));
    }
    m_server.SendWorldSnapshotTo(userId, world);
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
