#include "network/systems/ReplicationSystem.h"

#include "Logger.h"
#include "render/components/TransformComponent.h"

namespace Engine {
namespace Platform {
namespace Networking {
namespace Systems {

using Engine::Platform::Render::Components::TransformComponent;

namespace {

void RegisterDefaultReplicators(Engine::Net::ReplicationRegistry& registry) {
    Engine::Net::ReplicationComponentDescriptor transformDesc;
    transformDesc.id = Engine::Net::kTransformReplicationComponent;
    transformDesc.name = "Transform";
    transformDesc.serialize = [](const void* component, Engine::Net::NetBuffer& out) {
        const auto* transform = static_cast<const TransformComponent*>(component);
        out.WriteFloat(transform->position.x);
        out.WriteFloat(transform->position.y);
        out.WriteFloat(transform->position.z);
        out.WriteFloat(transform->rotation.x);
        out.WriteFloat(transform->rotation.y);
        out.WriteFloat(transform->rotation.z);
        out.WriteFloat(transform->scale.x);
        out.WriteFloat(transform->scale.y);
        out.WriteFloat(transform->scale.z);
        return true;
    };
    transformDesc.deserialize = [](const Engine::Net::NetBuffer& in, void* component) {
        auto* transform = static_cast<TransformComponent*>(component);
        Engine::Net::NetBuffer copy = in;
        copy.ResetRead();
        return copy.ReadFloat(transform->position.x) &&
               copy.ReadFloat(transform->position.y) &&
               copy.ReadFloat(transform->position.z) &&
               copy.ReadFloat(transform->rotation.x) &&
               copy.ReadFloat(transform->rotation.y) &&
               copy.ReadFloat(transform->rotation.z) &&
               copy.ReadFloat(transform->scale.x) &&
               copy.ReadFloat(transform->scale.y) &&
               copy.ReadFloat(transform->scale.z);
    };
    registry.Register(std::move(transformDesc));
}

} // namespace

ReplicationSystem::ReplicationSystem() {
    SetPriority(54);
    RegisterDefaultReplicators(m_registry);
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

    const EntityID ecsEntity = entityIt->second;
    if (const TransformComponent* transform = registry.GetComponent<TransformComponent>(ecsEntity)) {
        const Engine::Net::ReplicationComponentDescriptor* descriptor =
            m_registry.Find(Engine::Net::kTransformReplicationComponent);
        if (descriptor && descriptor->serialize) {
            Engine::Net::NetBuffer buffer;
            if (descriptor->serialize(transform, buffer)) {
                Engine::Net::ComponentSnapshot componentSnapshot;
                componentSnapshot.entityId = netEntityId;
                componentSnapshot.componentId = Engine::Net::kTransformReplicationComponent;
                componentSnapshot.tick = tick;
                componentSnapshot.data = buffer.Data();
                snapshot.components.push_back(std::move(componentSnapshot));
            }
        }
    }

    return snapshot;
}

void ReplicationSystem::ApplyEntitySnapshot(const Engine::Net::EntitySnapshot& snapshot,
                                            ECS::Registry& registry) const {
    if (m_receiveHandler) {
        m_receiveHandler(snapshot, registry);
        return;
    }

    const auto entityIt = m_netToEcs.find(snapshot.entityId);
    if (entityIt == m_netToEcs.end()) {
        return;
    }

    const EntityID ecsEntity = entityIt->second;
    for (const Engine::Net::ComponentSnapshot& componentSnapshot : snapshot.components) {
        const Engine::Net::ReplicationComponentDescriptor* descriptor =
            m_registry.Find(componentSnapshot.componentId);
        if (!descriptor || !descriptor->deserialize) {
            continue;
        }

        if (componentSnapshot.componentId == Engine::Net::kTransformReplicationComponent) {
            TransformComponent* transform = registry.GetComponent<TransformComponent>(ecsEntity);
            if (!transform) {
                transform = &registry.AddComponent<TransformComponent>(ecsEntity);
            }
            descriptor->deserialize(Engine::Net::NetBuffer(componentSnapshot.data), transform);
        }
    }
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
