#include "network/systems/NetworkSystem.h"

#include "Logger.h"
#include "core/ecs/Registry.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/TransformComponent.h"
#include "render/Model.h"

#include <glm/glm.hpp>
#include <cmath>

using EngineLogging::Logger;
using Engine::Platform::Render::Components::MaterialComponent;
using Engine::Platform::Render::Components::MeshComponent;
using Engine::Platform::Render::Components::TransformComponent;

namespace Engine {
namespace Platform {
namespace Networking {
namespace Systems {

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
        if (m_entitySnapshotHandler) {
            m_entitySnapshotHandler(snapshot);
        }
    };
    callbacks.onComponentSnapshot = [this](const Engine::Net::ComponentSnapshot& snapshot) {
        if (m_componentSnapshotHandler) {
            m_componentSnapshotHandler(snapshot);
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

glm::vec3 NetworkSystem::ResolveRemotePosition(const glm::vec3& networkPosition) const {
    if (!m_terrainModel) {
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
                                    const glm::vec3& position,
                                    const glm::vec3& rotation) {
    const glm::vec3 resolvedPosition = ResolveRemotePosition(position);
    RemotePlaybackState& state = m_remotePlayback[userId];
    state.targetPosition = resolvedPosition;
    state.targetRotation = rotation;

    if (!state.initialized) {
        state.displayPosition = resolvedPosition;
        state.displayRotation = rotation;
        state.initialized = true;
    }
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

        glm::vec3 resolvedDisplay = ResolveRemotePosition(state.displayPosition);
        if (std::abs(resolvedDisplay.y - state.displayPosition.y) < 0.2f) {
            state.displayPosition.y = resolvedDisplay.y;
        } else {
            state.displayPosition.y = glm::mix(state.displayPosition.y, state.targetPosition.y, blend);
        }

        transform->position = state.displayPosition;
        transform->rotation = state.displayRotation;
    }
}

void NetworkSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    m_client.Poll();

    for (Engine::Net::UserId userId : m_pendingRemovals) {
        RemoveRemoteEntity(userId, registry);
    }
    m_pendingRemovals.clear();

    for (auto it = m_pendingTransforms.begin(); it != m_pendingTransforms.end();) {
        const Engine::Net::UserId userId = it->first;
        const Engine::Net::TransformStatePayload& transform = it->second;
        GetOrCreateRemoteEntity(userId, registry);
        SetRemoteTarget(userId, transform.position, transform.rotationDegrees);
        it = m_pendingTransforms.erase(it);
    }

    InterpolateRemoteTransforms(deltaTime, registry);

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

} // namespace Systems
} // namespace Networking
} // namespace Platform
} // namespace Engine
