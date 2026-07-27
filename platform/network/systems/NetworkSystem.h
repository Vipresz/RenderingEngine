#pragma once

#include "core/Types.h"
#include "core/System.h"
#include "core/ecs/Entity.h"
#include "core/network/NetTypes.h"
#include "core/network/NetworkPayloads.h"
#include "core/network/ReplicationTypes.h"
#include "network/NetClient.h"
#include "render/components/TransformComponent.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace Engine {
namespace ECS {
class Registry;
}

namespace Platform {
namespace Render {
class Model;
}
namespace Networking {
namespace Systems {

using GenericDataHandler = std::function<void(const Engine::Net::GenericDataPayload&)>;
using WorldSnapshotHandler = std::function<void(const Engine::Net::WorldSnapshotPayload&)>;
using EntitySnapshotHandler = std::function<void(const Engine::Net::EntitySnapshot&)>;
using ComponentSnapshotHandler = std::function<void(const Engine::Net::ComponentSnapshot&)>;

// Syncs local player transforms to the server and applies remote player updates.
class NetworkSystem : public ECS::ISystem {
public:
    NetworkSystem();

    bool ConnectAndLogin(const std::string& host,
                         uint16_t port,
                         const std::string& displayName,
                         Engine::Net::UserId preferredUserId = Engine::Net::kInvalidUserId,
                         uint16_t udpPort = Engine::Net::kDefaultUdpPort);
    void Disconnect();

    void SetLocalPlayerEntity(EntityID entity) { m_localPlayerEntity = entity; }
    EntityID GetLocalPlayerEntity() const { return m_localPlayerEntity; }

    Engine::Net::UserId GetLocalUserId() const;
    bool IsLoggedIn() const;

    NetClient& GetClient() { return m_client; }
    const NetClient& GetClient() const { return m_client; }

    void SetGenericDataHandler(GenericDataHandler handler) { m_genericDataHandler = std::move(handler); }
    void SetWorldSnapshotHandler(WorldSnapshotHandler handler) { m_worldSnapshotHandler = std::move(handler); }
    void SetEntitySnapshotHandler(EntitySnapshotHandler handler) { m_entitySnapshotHandler = std::move(handler); }
    void SetComponentSnapshotHandler(ComponentSnapshotHandler handler) {
        m_componentSnapshotHandler = std::move(handler);
    }

    void SetUseUdpForMovement(bool enabled) { m_client.SetUseUdpForMovement(enabled); }
    bool SendEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot);
    bool SendWorldSnapshot(const Engine::Net::WorldSnapshotPayload& snapshot);

    // Shared mesh for remote players (e.g. male_model.obj). Scale matches local character.
    void SetRemotePlayerModel(Engine::Platform::Render::Model* model, float scale = 1.0f);

    // Terrain used to snap remote players to the local ground height at their X/Z.
    void SetTerrainModel(Engine::Platform::Render::Model* terrain) { m_terrainModel = terrain; }

    using RemotePlayerSetupHandler =
        std::function<void(ECS::Registry&, EntityID, Engine::Net::UserId, const std::string&)>;
    void SetRemotePlayerSetupHandler(RemotePlayerSetupHandler handler);

    void CollectRemotePlayerEntities(std::vector<EntityID>& out) const;

    const std::string& GetPeerDisplayName(Engine::Net::UserId userId) const;

    // When true (default), send transform every frame after gameplay systems run.
    void SetGreedySend(bool enabled) { m_greedySend = enabled; }
    void SetSendRateHz(float hz);

    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;

private:
    struct RemotePlaybackState {
        glm::vec3 displayPosition{0.0f};
        glm::vec3 targetPosition{0.0f};
        glm::vec3 displayRotation{0.0f};
        glm::vec3 targetRotation{0.0f};
        bool initialized = false;
    };

    EntityID GetOrCreateRemoteEntity(Engine::Net::UserId userId, ECS::Registry& registry);
    void AttachRemotePlayerVisual(EntityID entity, ECS::Registry& registry);
    void RemoveRemoteEntity(Engine::Net::UserId userId, ECS::Registry& registry);
    void SendLocalTransform(ECS::Registry& registry, uint64_t tickNumber);
    bool ShouldSendTransform(const Engine::Platform::Render::Components::TransformComponent& transform) const;
    glm::vec3 ResolveRemotePosition(const glm::vec3& networkPosition) const;
    void SetRemoteTarget(Engine::Net::UserId userId, const glm::vec3& position, const glm::vec3& rotation);
    void InterpolateRemoteTransforms(float deltaTime, ECS::Registry& registry);
    static float LerpAngleDegrees(float from, float to, float t);
    static glm::vec3 LerpAnglesDegrees(const glm::vec3& from, const glm::vec3& to, float t);

    NetClient m_client;
    EntityID m_localPlayerEntity = INVALID_ENTITY;
    std::unordered_map<Engine::Net::UserId, EntityID> m_remoteEntities;
    std::unordered_map<Engine::Net::UserId, Engine::Net::TransformStatePayload> m_pendingTransforms;
    std::unordered_map<Engine::Net::UserId, RemotePlaybackState> m_remotePlayback;
    std::vector<Engine::Net::UserId> m_pendingRemovals;
    GenericDataHandler m_genericDataHandler;
    WorldSnapshotHandler m_worldSnapshotHandler;
    EntitySnapshotHandler m_entitySnapshotHandler;
    ComponentSnapshotHandler m_componentSnapshotHandler;
    Engine::Platform::Render::Model* m_remotePlayerModel = nullptr;
    Engine::Platform::Render::Model* m_terrainModel = nullptr;
    float m_remotePlayerScale = 1.0f;
    std::unordered_map<Engine::Net::UserId, std::string> m_peerDisplayNames;
    RemotePlayerSetupHandler m_remotePlayerSetupHandler;
    bool m_greedySend = true;
    float m_sendAccumulator = 0.0f;
    float m_sendIntervalSeconds = 0.0f;
    float m_interpolationSharpness = 18.0f;
    glm::vec3 m_lastSentPosition{-99999.0f};
    glm::vec3 m_lastSentRotation{-99999.0f};
};

} // namespace Systems
} // namespace Networking
} // namespace Platform
} // namespace Engine
