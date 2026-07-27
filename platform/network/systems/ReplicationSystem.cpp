#include "network/systems/ReplicationSystem.h"

#include "Logger.h"

namespace Engine {
namespace Platform {
namespace Networking {
namespace Systems {

ReplicationSystem::ReplicationSystem() {
    SetPriority(54);
}

void ReplicationSystem::RegisterReplicatedEntity(Engine::Net::NetEntityId netEntityId, EntityID ecsEntity) {
    m_netToEcs[netEntityId] = ecsEntity;
}

void ReplicationSystem::UnregisterReplicatedEntity(Engine::Net::NetEntityId netEntityId) {
    m_netToEcs.erase(netEntityId);
}

Engine::Net::EntitySnapshot ReplicationSystem::BuildEntitySnapshot(Engine::Net::NetEntityId netEntityId,
                                                                     ECS::Registry& registry,
                                                                     uint64_t tick) const {
    Engine::Net::EntitySnapshot snapshot;
    snapshot.entityId = netEntityId;
    snapshot.tick = tick;

    const auto entityIt = m_netToEcs.find(netEntityId);
    if (entityIt == m_netToEcs.end()) {
        return snapshot;
    }

    (void)registry;
  (void)entityIt;
    // Game registers component serializers via ReplicationRegistry and extends this system
    // to pull component data from ECS pools.
    return snapshot;
}

void ReplicationSystem::ApplyEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot,
                                            ECS::Registry& registry) const {
    if (m_receiveHandler) {
        m_receiveHandler(snapshot, registry);
        return;
    }

    (void)snapshot;
    (void)registry;
}

void ReplicationSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    m_sendAccumulator += deltaTime;
    if (!m_sendHandler || m_sendIntervalSeconds <= 0.0f || m_sendAccumulator < m_sendIntervalSeconds) {
        return;
    }
    m_sendAccumulator = 0.0f;

    for (const auto& entry : m_netToEcs) {
        const Engine::Net::EntitySnapshot snapshot = BuildEntitySnapshot(entry.first, registry, tickNumber);
        if (!snapshot.components.empty()) {
            m_sendHandler(snapshot);
        }
    }
}

} // namespace Systems
} // namespace Networking
} // namespace Platform
} // namespace Engine
