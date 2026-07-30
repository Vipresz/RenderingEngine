#include "network/systems/NetworkSystem.h"

#include "Logger.h"
#include "core/ecs/Registry.h"
#include "core/network/CharacterMovementSimulator.h"
#include "core/network/CharacterMovementState.h"
#include "core/network/InputBuffer.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/TransformComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/Model.h"

#include <glm/glm.hpp>
#include <cmath>
#include <chrono>
#include <thread>

using EngineLogging::Logger;
using Engine::Platform::Render::Components::MaterialComponent;
using Engine::Platform::Render::Components::MeshComponent;
using Engine::Platform::Render::Components::TransformComponent;
using Engine::Platform::Render::Components::AnimationComponent;

namespace Engine {
namespace Platform {
namespace Networking {
namespace Systems {

namespace {

void ApplyRemoteAnimationFromPlayback(AnimationComponent& anim,
                                      bool isJumping,
                                      bool jumpAnimationActive,
                                      float jumpVelocity,
                                      float airVelocityX,
                                      float airVelocityZ,
                                      float groundLevel,
                                      uint8_t animationState) {
    anim.isJumping = isJumping;
    anim.jumpAnimationActive = jumpAnimationActive;
    anim.jumpVelocity = jumpVelocity;
    anim.airVelocityX = airVelocityX;
    anim.airVelocityZ = airVelocityZ;
    anim.groundLevel = groundLevel;
    anim.locomotionSpeed = std::sqrt(airVelocityX * airVelocityX + airVelocityZ * airVelocityZ);

    switch (static_cast<Engine::Net::MovementAnimationState>(animationState)) {
        case Engine::Net::MovementAnimationState::Walking:
            anim.currentState = AnimationComponent::AnimationState::Walking;
            break;
        case Engine::Net::MovementAnimationState::Running:
            anim.currentState = AnimationComponent::AnimationState::Running;
            break;
        case Engine::Net::MovementAnimationState::Jumping:
            anim.currentState = AnimationComponent::AnimationState::Jumping;
            break;
        case Engine::Net::MovementAnimationState::TurningLeft:
            anim.currentState = AnimationComponent::AnimationState::TurningLeft;
            break;
        case Engine::Net::MovementAnimationState::TurningRight:
            anim.currentState = AnimationComponent::AnimationState::TurningRight;
            break;
        case Engine::Net::MovementAnimationState::Idle:
        default:
            if (isJumping || jumpAnimationActive) {
                anim.currentState = AnimationComponent::AnimationState::Jumping;
            } else {
                anim.currentState = AnimationComponent::AnimationState::Idle;
            }
            break;
    }
    anim.isRunning = anim.currentState == AnimationComponent::AnimationState::Running;
}

} // namespace

NetworkSystem::NetworkSystem() {
    // After Input (0), Camera (10), equipment (24), Animation (50) so we send the final pose.
    SetPriority(55);

    ClientCallbacks callbacks;
    callbacks.onPeerJoined = [this](Engine::Net::UserId userId, const std::string& displayName) {
        LOG_INFO_FMT("NetworkSystem", "Peer joined: %u (%s)", userId, displayName.c_str());
        m_peerDisplayNames[userId] = displayName;
    };
    callbacks.onPeerLeft = [this](Engine::Net::UserId userId) {
        LOG_INFO_FMT("NetworkSystem", "Peer left: %u", userId);
        m_pendingRemovals.push_back(userId);
    };
    callbacks.onTransformState = [this](const Engine::Net::TransformStatePayload& transform) {
        if (transform.userId == m_client.GetUserId()) {
            m_localServerCorrection = transform;
            m_hasLocalServerCorrection = true;
            return;
        }
        m_pendingTransforms[transform.userId] = transform;
    };
    callbacks.onGenericData = [this](const Engine::Net::GenericDataPayload& payload) {
        if (m_genericDataHandler) {
            m_genericDataHandler(payload);
        }
    };
    callbacks.onWorldSnapshot = [this](const Engine::Net::WorldSnapshotPayload& snapshot) {
        if (m_worldSnapshotHandler) {
            m_worldSnapshotHandler(snapshot);
        }
    };
    callbacks.onEntitySnapshot = [this](const Engine::Net::EntitySnapshot& snapshot) {
        m_pendingEntitySnapshots.push_back(snapshot);
    };
    callbacks.onComponentSnapshot = [this](const Engine::Net::ComponentSnapshot& snapshot) {
        m_pendingComponentSnapshots.push_back(snapshot);
    };
    callbacks.onMovementAck = [this](const Engine::Net::MovementAckPayload& ack) {
        std::lock_guard<std::mutex> lock(m_movementAckMutex);
        if (m_pendingMovementAckSignal != nullptr && ack.tick == m_pendingMovementAckTick &&
            ack.userId == m_client.GetUserId()) {
            m_pendingMovementAckSignal->store(true, std::memory_order_release);
        }
    };
    m_client.SetCallbacks(std::move(callbacks));
}

bool NetworkSystem::ConnectAndLogin(const std::string& host,
                                    uint16_t port,
                                    const std::string& displayName,
                                    Engine::Net::UserId preferredUserId,
                                    uint16_t udpPort) {
    if (!m_client.Connect(host, port)) {
        return false;
    }
    if (!m_client.Login(displayName, preferredUserId)) {
        return false;
    }
    m_client.ConnectUdp(host, udpPort);
    return m_client.RegisterUdpSession();
}

void NetworkSystem::Disconnect() {
    m_client.Disconnect();
    m_remoteEntities.clear();
    m_pendingTransforms.clear();
    m_remotePlayback.clear();
    m_peerDisplayNames.clear();
    m_pendingEntitySnapshots.clear();
    m_pendingComponentSnapshots.clear();
    m_lastSentCommandTick = 0;
    m_lastAcknowledgedTick = 0;
}

Engine::Net::UserId NetworkSystem::GetLocalUserId() const {
    return m_client.GetUserId();
}

bool NetworkSystem::IsLoggedIn() const {
    return m_client.IsLoggedIn();
}

void NetworkSystem::SetRemotePlayerModel(Engine::Platform::Render::Model* model, float scale) {
    m_remotePlayerModel = model;
    m_remotePlayerScale = scale;
}

void NetworkSystem::SetRemotePlayerSetupHandler(RemotePlayerSetupHandler handler) {
    m_remotePlayerSetupHandler = std::move(handler);
}

const std::string& NetworkSystem::GetPeerDisplayName(Engine::Net::UserId userId) const {
    static const std::string kEmpty;
    const auto it = m_peerDisplayNames.find(userId);
    return it == m_peerDisplayNames.end() ? kEmpty : it->second;
}

void NetworkSystem::CollectRemotePlayerEntities(std::vector<EntityID>& out) const {
    out.clear();
    out.reserve(m_remoteEntities.size());
    for (const auto& entry : m_remoteEntities) {
        out.push_back(entry.second);
    }
}

EntityID NetworkSystem::EnsureRemoteEntity(Engine::Net::UserId userId, ECS::Registry& registry) {
    return GetOrCreateRemoteEntity(userId, registry);
}

EntityID NetworkSystem::TryGetPlayerEntity(Engine::Net::UserId userId) const {
    if (userId == Engine::Net::kInvalidUserId) {
        return INVALID_ENTITY;
    }
    if (userId == GetLocalUserId() && m_localPlayerEntity != INVALID_ENTITY) {
        return m_localPlayerEntity;
    }
    const auto it = m_remoteEntities.find(userId);
    return it != m_remoteEntities.end() ? it->second : INVALID_ENTITY;
}

Engine::Net::UserId NetworkSystem::TryGetUserIdForEntity(EntityID entity) const {
    if (entity == INVALID_ENTITY) {
        return Engine::Net::kInvalidUserId;
    }
    if (entity == m_localPlayerEntity) {
        return GetLocalUserId();
    }
    for (const auto& entry : m_remoteEntities) {
        if (entry.second == entity) {
            return entry.first;
        }
    }
    return Engine::Net::kInvalidUserId;
}

void NetworkSystem::SetSendRateHz(float hz) {
    if (hz <= 0.0f) {
        m_sendIntervalSeconds = 0.0f;
        m_greedySend = true;
        return;
    }
    m_greedySend = false;
    m_sendIntervalSeconds = 1.0f / hz;
}

bool NetworkSystem::SendEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot) {
    return m_client.SendEntitySnapshot(snapshot);
}

bool NetworkSystem::SendWorldSnapshot(const Engine::Net::WorldSnapshotPayload& snapshot) {
    return m_client.SendWorldSnapshot(snapshot);
}

void NetworkSystem::SendMovementCommand(const Engine::Net::MovementCommand& command) {
    if (!m_client.IsLoggedIn()) {
        return;
    }
    m_client.SendMovementCommand(command);
}

void NetworkSystem::Shutdown(ECS::Registry& registry) {
    for (const auto& entry : m_remoteEntities) {
        registry.DestroyEntity(entry.second);
    }
    m_remoteEntities.clear();
    m_pendingTransforms.clear();
    m_remotePlayback.clear();
    Disconnect();
}

EntityID NetworkSystem::GetOrCreateRemoteEntity(Engine::Net::UserId userId, ECS::Registry& registry) {
    auto it = m_remoteEntities.find(userId);
    if (it != m_remoteEntities.end() && registry.HasComponent<TransformComponent>(it->second)) {
        AttachRemotePlayerVisual(it->second, registry);
        return it->second;
    }

    const ECS::Entity entity = registry.CreateEntity();
    registry.AddComponent<TransformComponent>(entity.id);
    AttachRemotePlayerVisual(entity.id, registry);
    m_remoteEntities[userId] = entity.id;

    if (m_peerEntityHandler) {
        m_peerEntityHandler(userId, entity.id);
    }

    if (m_remotePlayerSetupHandler) {
        const auto nameIt = m_peerDisplayNames.find(userId);
        const std::string& displayName =
            nameIt != m_peerDisplayNames.end() ? nameIt->second : ("Player" + std::to_string(userId));
        m_remotePlayerSetupHandler(registry, entity.id, userId, displayName);
    }

    return entity.id;
}

void NetworkSystem::AttachRemotePlayerVisual(EntityID entity, ECS::Registry& registry) {
    if (!m_remotePlayerModel || !registry.HasComponent<TransformComponent>(entity)) {
        return;
    }

    if (!registry.HasComponent<MeshComponent>(entity)) {
        auto& mesh = registry.AddComponent<MeshComponent>(entity);
        mesh.model = m_remotePlayerModel;
    }

    if (!registry.HasComponent<MaterialComponent>(entity)) {
        auto& material = registry.AddComponent<MaterialComponent>(entity);
        material.specularIntensity = 0.5f;
        material.shininess = 32.0f;
    }

    if (TransformComponent* transform = registry.GetComponent<TransformComponent>(entity)) {
        transform->SetScale(m_remotePlayerScale);
    }
}

void NetworkSystem::RemoveRemoteEntity(Engine::Net::UserId userId, ECS::Registry& registry) {
    auto it = m_remoteEntities.find(userId);
    if (it == m_remoteEntities.end()) {
        return;
    }
    registry.DestroyEntity(it->second);
    m_remoteEntities.erase(it);
    m_pendingTransforms.erase(userId);
    m_remotePlayback.erase(userId);
    m_peerDisplayNames.erase(userId);
}

void NetworkSystem::SendLocalTransform(ECS::Registry& registry, uint64_t tickNumber) {
    if (!m_client.IsLoggedIn() || m_localPlayerEntity == INVALID_ENTITY) {
        return;
    }

    if (!registry.HasComponent<TransformComponent>(m_localPlayerEntity)) {
        return;
    }

    const TransformComponent* transform = registry.GetComponent<TransformComponent>(m_localPlayerEntity);
    if (!transform) {
        return;
    }

    Engine::Net::TransformStatePayload payload;
    payload.userId = m_client.GetUserId();
    payload.tick = tickNumber;
    payload.position = transform->position;
    payload.rotationDegrees = transform->rotation;
    if (const AnimationComponent* anim =
            registry.GetComponent<AnimationComponent>(m_localPlayerEntity)) {
        payload.isJumping = anim->isJumping;
        payload.jumpAnimationActive = anim->jumpAnimationActive;
        payload.jumpVelocity = anim->jumpVelocity;
        payload.airVelocityX = anim->airVelocityX;
        payload.airVelocityZ = anim->airVelocityZ;
        payload.groundLevel = anim->groundLevel;
        payload.animationState = static_cast<uint8_t>(anim->currentState);
    }
    m_client.SendTransformState(payload);
    m_lastSentPosition = payload.position;
    m_lastSentRotation = payload.rotationDegrees;
}

bool NetworkSystem::ShouldSendTransform(const TransformComponent& transform) const {
    if (m_greedySend) {
        return true;
    }

    constexpr float kPositionEpsilonSq = 0.0001f * 0.0001f;
    constexpr float kRotationEpsilonSq = 0.01f * 0.01f;
    const glm::vec3 positionDelta = transform.position - m_lastSentPosition;
    const glm::vec3 rotationDelta = transform.rotation - m_lastSentRotation;
    return glm::dot(positionDelta, positionDelta) > kPositionEpsilonSq ||
           glm::dot(rotationDelta, rotationDelta) > kRotationEpsilonSq;
}

float NetworkSystem::LerpAngleDegrees(float from, float to, float t) {
    float delta = to - from;
    while (delta > 180.0f) {
        delta -= 360.0f;
    }
    while (delta < -180.0f) {
        delta += 360.0f;
    }
    return from + delta * t;
}

glm::vec3 NetworkSystem::LerpAnglesDegrees(const glm::vec3& from, const glm::vec3& to, float t) {
    return glm::vec3(LerpAngleDegrees(from.x, to.x, t),
                     LerpAngleDegrees(from.y, to.y, t),
                     LerpAngleDegrees(from.z, to.z, t));
}

glm::vec3 NetworkSystem::ResolveRemotePosition(const glm::vec3& networkPosition,
                                                 bool isJumping) const {
    if (!m_terrainModel || isJumping) {
        // Trust networked Y while airborne — ground snapping here caused jump shake
        // and made nameplates appear doubled (ground copy + air copy).
        return networkPosition;
    }

    const float terrainHeight =
        m_terrainModel->GetHeightAt(networkPosition.x, networkPosition.z);
    constexpr float kAirborneThreshold = 0.2f;
    if (networkPosition.y > terrainHeight + kAirborneThreshold) {
        return networkPosition;
    }

    glm::vec3 resolved = networkPosition;
    resolved.y = terrainHeight;
    return resolved;
}

void NetworkSystem::SetRemoteTarget(Engine::Net::UserId userId,
                                    const Engine::Net::TransformStatePayload& transform) {
    const glm::vec3 resolvedPosition =
        ResolveRemotePosition(transform.position, transform.isJumping);
    RemotePlaybackState& state = m_remotePlayback[userId];
    state.targetPosition = resolvedPosition;
    state.targetRotation = transform.rotationDegrees;
    state.isJumping = transform.isJumping;
    state.jumpAnimationActive = transform.jumpAnimationActive;
    state.jumpVelocity = transform.jumpVelocity;
    state.airVelocityX = transform.airVelocityX;
    state.airVelocityZ = transform.airVelocityZ;
    state.groundLevel = transform.groundLevel;
    state.animationState = transform.animationState;

    if (!state.initialized) {
        state.displayPosition = resolvedPosition;
        state.displayRotation = transform.rotationDegrees;
        state.initialized = true;
    }
}

void NetworkSystem::ApplyLocalServerCorrection(ECS::Registry& registry) {
    if (!m_hasLocalServerCorrection || m_localPlayerEntity == INVALID_ENTITY) {
        return;
    }

    if (m_clientPredictionEnabled && m_inputBuffer) {
        ReconcileWithRollback(registry, m_localServerCorrection);
        m_hasLocalServerCorrection = false;
        return;
    }

    TransformComponent* transform = registry.GetComponent<TransformComponent>(m_localPlayerEntity);
    if (!transform) {
        m_hasLocalServerCorrection = false;
        return;
    }

    constexpr float kSnapDistanceSq = 0.5f * 0.5f;
    const glm::vec3 delta = m_localServerCorrection.position - transform->position;
    if (glm::dot(delta, delta) > kSnapDistanceSq) {
        transform->position = m_localServerCorrection.position;
        transform->rotation = m_localServerCorrection.rotationDegrees;
    } else {
        transform->position = glm::mix(transform->position, m_localServerCorrection.position, 0.25f);
        transform->rotation =
            LerpAnglesDegrees(transform->rotation, m_localServerCorrection.rotationDegrees, 0.25f);
    }

    m_hasLocalServerCorrection = false;
}

void NetworkSystem::ReconcileWithRollback(ECS::Registry& registry,
                                          const Engine::Net::TransformStatePayload& serverState) {
    if (m_localPlayerEntity == INVALID_ENTITY || !m_inputBuffer) {
        return;
    }

    TransformComponent* transform = registry.GetComponent<TransformComponent>(m_localPlayerEntity);
    if (!transform) {
        return;
    }

    auto* animComp = registry.GetComponent<AnimationComponent>(m_localPlayerEntity);

    if (serverState.tick <= m_lastAcknowledgedTick) {
        return;
    }
    m_lastAcknowledgedTick = serverState.tick;

    // Seed rollback from the server's documented movement snapshot — never from the
    // live client jump fields (those are ahead of serverState.tick and cause Y thrash).
    Engine::Net::CharacterMovementState state;
    state.position = serverState.position;
    state.yawDegrees = serverState.rotationDegrees.y;
    state.isJumping = serverState.isJumping;
    state.jumpVelocity = serverState.jumpVelocity;
    state.groundLevel = serverState.groundLevel;
    state.airVelocityX = serverState.airVelocityX;
    state.airVelocityZ = serverState.airVelocityZ;
    state.jumpAnimationActive = serverState.jumpAnimationActive;
    state.animationState =
        static_cast<Engine::Net::MovementAnimationState>(serverState.animationState);
    if (animComp) {
        state.gravity = animComp->gravity;
        state.jumpInitialVelocity = animComp->jumpInitialVelocity;
    }

    const Engine::Net::TerrainHeightSampler terrainSampler =
        m_terrainModel ? Engine::Net::TerrainHeightSampler(
                             [terrain = m_terrainModel](float x, float z) {
                                 return terrain->GetHeightAt(x, z);
                             })
                       : Engine::Net::TerrainHeightSampler();

    m_inputBuffer->ForEachAfter(serverState.tick, [&](const Engine::Net::MovementCommand& command) {
        Engine::Net::CharacterMovementSimulator::SimulateStep(state, command, terrainSampler);
    });

    transform->position = state.position;
    transform->SetYaw(state.yawDegrees);
    transform->rotation.x = serverState.rotationDegrees.x;
    transform->rotation.z = serverState.rotationDegrees.z;

    if (!animComp) {
        return;
    }

    animComp->isJumping = state.isJumping;
    animComp->jumpVelocity = state.jumpVelocity;
    animComp->groundLevel = state.groundLevel;
    animComp->airVelocityX = state.airVelocityX;
    animComp->airVelocityZ = state.airVelocityZ;
    animComp->jumpAnimationActive = state.jumpAnimationActive;
    animComp->locomotionSpeed =
        std::sqrt(state.airVelocityX * state.airVelocityX + state.airVelocityZ * state.airVelocityZ);
    switch (state.animationState) {
        case Engine::Net::MovementAnimationState::Walking:
            animComp->currentState = AnimationComponent::AnimationState::Walking;
            break;
        case Engine::Net::MovementAnimationState::Running:
            animComp->currentState = AnimationComponent::AnimationState::Running;
            break;
        case Engine::Net::MovementAnimationState::Jumping:
            animComp->currentState = AnimationComponent::AnimationState::Jumping;
            break;
        case Engine::Net::MovementAnimationState::TurningLeft:
            animComp->currentState = AnimationComponent::AnimationState::TurningLeft;
            break;
        case Engine::Net::MovementAnimationState::TurningRight:
            animComp->currentState = AnimationComponent::AnimationState::TurningRight;
            break;
        case Engine::Net::MovementAnimationState::Idle:
        default:
            animComp->currentState = AnimationComponent::AnimationState::Idle;
            break;
    }
    animComp->isRunning = animComp->currentState == AnimationComponent::AnimationState::Running;
}

void NetworkSystem::InterpolateRemoteTransforms(float deltaTime, ECS::Registry& registry) {
    if (m_remotePlayback.empty()) {
        return;
    }

    const float blend = 1.0f - std::exp(-m_interpolationSharpness * deltaTime);
    for (const auto& entry : m_remoteEntities) {
        const Engine::Net::UserId userId = entry.first;
        const EntityID entity = entry.second;
        auto playbackIt = m_remotePlayback.find(userId);
        if (playbackIt == m_remotePlayback.end() || !playbackIt->second.initialized) {
            continue;
        }

        RemotePlaybackState& state = playbackIt->second;
        TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform) {
            continue;
        }

        state.displayPosition = glm::mix(state.displayPosition, state.targetPosition, blend);
        state.displayRotation = LerpAnglesDegrees(state.displayRotation, state.targetRotation, blend);

        if (!state.isJumping) {
            glm::vec3 resolvedDisplay =
                ResolveRemotePosition(state.displayPosition, false);
            if (std::abs(resolvedDisplay.y - state.displayPosition.y) < 0.2f) {
                state.displayPosition.y = resolvedDisplay.y;
            } else {
                state.displayPosition.y =
                    glm::mix(state.displayPosition.y, state.targetPosition.y, blend);
            }
        }

        transform->position = state.displayPosition;
        transform->rotation = state.displayRotation;

        AnimationComponent* anim = registry.GetComponent<AnimationComponent>(entity);
        if (!anim) {
            anim = &registry.AddComponent<AnimationComponent>(entity);
        }
        ApplyRemoteAnimationFromPlayback(*anim,
                                         state.isJumping,
                                         state.jumpAnimationActive,
                                         state.jumpVelocity,
                                         state.airVelocityX,
                                         state.airVelocityZ,
                                         state.groundLevel,
                                         state.animationState);
    }
}

void NetworkSystem::FlushDeferredSnapshots() {
    if (m_entitySnapshotHandler) {
        for (const Engine::Net::EntitySnapshot& snapshot : m_pendingEntitySnapshots) {
            m_entitySnapshotHandler(snapshot);
        }
    }
    m_pendingEntitySnapshots.clear();

    if (m_componentSnapshotHandler) {
        for (const Engine::Net::ComponentSnapshot& snapshot : m_pendingComponentSnapshots) {
            m_componentSnapshotHandler(snapshot);
        }
    }
    m_pendingComponentSnapshots.clear();
}

void NetworkSystem::PollNetwork() {
    m_client.Poll();
}

void NetworkSystem::StageEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot) {
    m_pendingEntitySnapshots.push_back(snapshot);
}

void NetworkSystem::StageComponentSnapshot(const Engine::Net::ComponentSnapshot& snapshot) {
    m_pendingComponentSnapshots.push_back(snapshot);
}

void NetworkSystem::ProcessNetworkFrame(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    FlushDeferredSnapshots();

    for (Engine::Net::UserId userId : m_pendingRemovals) {
        RemoveRemoteEntity(userId, registry);
    }
    m_pendingRemovals.clear();

    for (auto it = m_pendingTransforms.begin(); it != m_pendingTransforms.end();) {
        const Engine::Net::UserId userId = it->first;
        const Engine::Net::TransformStatePayload& transform = it->second;
        GetOrCreateRemoteEntity(userId, registry);
        SetRemoteTarget(userId, transform);
        it = m_pendingTransforms.erase(it);
    }

    ApplyLocalServerCorrection(registry);
    InterpolateRemoteTransforms(deltaTime, registry);

    if (m_authoritativeClient) {
        if (m_inputBuffer && m_client.IsLoggedIn()) {
            uint64_t highestSent = m_lastSentCommandTick;
            m_inputBuffer->ForEachAfter(m_lastSentCommandTick, [&](const Engine::Net::MovementCommand& command) {
                SendMovementCommand(command);
                if (command.tick > highestSent) {
                    highestSent = command.tick;
                }
            });
            m_lastSentCommandTick = highestSent;
        }
        return;
    }

    m_sendAccumulator += deltaTime;
    const bool intervalElapsed =
        m_sendIntervalSeconds <= 0.0f || m_sendAccumulator >= m_sendIntervalSeconds;
    if (intervalElapsed && registry.HasComponent<TransformComponent>(m_localPlayerEntity)) {
        const TransformComponent* transform =
            registry.GetComponent<TransformComponent>(m_localPlayerEntity);
        if (transform && ShouldSendTransform(*transform)) {
            if (m_sendIntervalSeconds > 0.0f) {
                m_sendAccumulator = 0.0f;
            }
            SendLocalTransform(registry, tickNumber);
        }
    }
}

void NetworkSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    PollNetwork();
    ProcessNetworkFrame(deltaTime, registry, tickNumber);
}

void NetworkSystem::ArmMovementAckWait(uint64_t commandTick, std::atomic<bool>* signal) {
    std::lock_guard<std::mutex> lock(m_movementAckMutex);
    m_pendingMovementAckTick = commandTick;
    m_pendingMovementAckSignal = signal;
    if (signal != nullptr) {
        signal->store(false, std::memory_order_release);
    }
}

void NetworkSystem::DisarmMovementAckWait() {
    std::lock_guard<std::mutex> lock(m_movementAckMutex);
    m_pendingMovementAckSignal = nullptr;
    m_pendingMovementAckTick = 0;
}

bool NetworkSystem::PollUntilMovementAck(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
        std::atomic<bool>* signal = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_movementAckMutex);
            signal = m_pendingMovementAckSignal;
        }
        if (signal != nullptr && signal->load(std::memory_order_acquire)) {
            return true;
        }
        PollNetwork();
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::yield();
    }
}

bool NetworkSystem::WaitForMovementAck(uint64_t commandTick, std::chrono::milliseconds timeout) {
    std::atomic<bool> received{false};
    ArmMovementAckWait(commandTick, &received);
    const bool acked = PollUntilMovementAck(timeout);
    DisarmMovementAckWait();
    return acked;
}

} // namespace Systems
} // namespace Networking
} // namespace Platform
} // namespace Engine
