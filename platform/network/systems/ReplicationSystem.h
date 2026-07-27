#pragma once

#include "core/System.h"
#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "core/network/ReplicationTypes.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Platform {
namespace Networking {
namespace Systems {

using SnapshotSendHandler = std::function<void(const Engine::Net::EntitySnapshot&)>;
using SnapshotReceiveHandler = std::function<void(const Engine::Net::EntitySnapshot&, ECS::Registry&)>;

// Builds and applies component/entity replication snapshots for ECS entities.
class ReplicationSystem : public ECS::ISystem {
public:
    ReplicationSystem();

    Engine::Net::ReplicationRegistry& GetRegistry() { return m_registry; }
    const Engine::Net::ReplicationRegistry& GetRegistry() const { return m_registry; }

    void RegisterReplicatedEntity(Engine::Net::NetEntityId netEntityId, EntityID ecsEntity);
    void UnregisterReplicatedEntity(Engine::Net::NetEntityId netEntityId);

    void SetSnapshotSendHandler(SnapshotSendHandler handler) { m_sendHandler = std::move(handler); }
    void SetSnapshotReceiveHandler(SnapshotReceiveHandler handler) { m_receiveHandler = std::move(handler); }

    Engine::Net::EntitySnapshot BuildEntitySnapshot(Engine::Net::NetEntityId netEntityId,
                                                    ECS::Registry& registry,
                                                    uint64_t tick) const;
    void ApplyEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot, ECS::Registry& registry) const;

    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;

private:
    Engine::Net::ReplicationRegistry m_registry;
    std::unordered_map<Engine::Net::NetEntityId, EntityID> m_netToEcs;
    SnapshotSendHandler m_sendHandler;
    SnapshotReceiveHandler m_receiveHandler;
    float m_sendAccumulator = 0.0f;
    float m_sendIntervalSeconds = 0.1f;
};

} // namespace Systems
} // namespace Networking
} // namespace Platform
} // namespace Engine
