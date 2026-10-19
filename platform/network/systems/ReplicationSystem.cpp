#include "network/systems/ReplicationSystem.h"

#include "Logger.h"
#include "core/network/ColliderReplication.h"
#include "physics/components/ColliderComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/components/TransformComponent.h"

namespace Engine {
namespace Platform {
namespace Networking {
namespace Systems {

using Engine::Platform::Render::Components::AnimationComponent;
using Engine::Platform::Render::Components::TransformComponent;
using Engine::Physics::Components::ColliderComponent;

namespace {

void WriteAnimationReplicationBlob(const AnimationComponent& anim, Engine::Net::NetBuffer& out) {
    out.WriteU8(static_cast<uint8_t>(anim.currentState));
    out.WriteU8(anim.isJumping ? 1 : 0);
    out.WriteU8(anim.isRunning ? 1 : 0);
    out.WriteFloat(anim.currentAnimationTime);
    out.WriteFloat(anim.locomotionSpeed);
    out.WriteU8(anim.isLooping ? 1 : 0);
    out.WriteU8(anim.jumpAnimationActive ? 1 : 0);
    out.WriteU8(anim.landingActive ? 1 : 0);
    out.WriteFloat(anim.airVelocityX);
    out.WriteFloat(anim.airVelocityZ);
}

bool ReadAnimationReplicationBlob(const Engine::Net::NetBuffer& in, AnimationComponent& anim) {
    Engine::Net::NetBuffer copy = in;
    copy.ResetRead();
    uint8_t state = 0;
    uint8_t jumping = 0;
    uint8_t running = 0;
    if (!copy.ReadU8(state) || !copy.ReadU8(jumping) || !copy.ReadU8(running)) {
        return false;
    }
    anim.currentState = static_cast<AnimationComponent::AnimationState>(state);
    anim.isJumping = jumping != 0;
    anim.isRunning = running != 0;

    float clipTime = 0.0f;
    float locomotionSpeed = 0.0f;
    uint8_t looping = 1;
    uint8_t jumpAnim = 0;
    uint8_t landing = 0;
    float airX = 0.0f;
    float airZ = 0.0f;
    if (!copy.ReadFloat(clipTime) || !copy.ReadFloat(locomotionSpeed) || !copy.ReadU8(looping) ||
        !copy.ReadU8(jumpAnim) || !copy.ReadU8(landing) || !copy.ReadFloat(airX) ||
        !copy.ReadFloat(airZ)) {
        return true;
    }
    anim.currentAnimationTime = clipTime;
    anim.locomotionSpeed = locomotionSpeed;
    anim.isLooping = looping != 0;
    anim.jumpAnimationActive = jumpAnim != 0;
    anim.landingActive = landing != 0;
    anim.airVelocityX = airX;
    anim.airVelocityZ = airZ;
    return true;
}

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
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
        const bool ok = copy.ReadFloat(transform->position.x) &&
                        copy.ReadFloat(transform->position.y) &&
                        copy.ReadFloat(transform->position.z) &&
                        copy.ReadFloat(transform->rotation.x) &&
                        copy.ReadFloat(transform->rotation.y) &&
                        copy.ReadFloat(transform->rotation.z) &&
                        copy.ReadFloat(scaleX) &&
                        copy.ReadFloat(scaleY) &&
                        copy.ReadFloat(scaleZ);
        (void)scaleX;
        (void)scaleY;
        (void)scaleZ;
        return ok;
    };
    registry.Register(std::move(transformDesc));

    Engine::Net::ReplicationComponentDescriptor animationDesc;
    animationDesc.id = Engine::Net::kAnimationReplicationComponent;
    animationDesc.name = "Animation";
    animationDesc.serialize = [](const void* component, Engine::Net::NetBuffer& out) {
        WriteAnimationReplicationBlob(*static_cast<const AnimationComponent*>(component), out);
        return true;
    };
    animationDesc.deserialize = [](const Engine::Net::NetBuffer& in, void* component) {
        return ReadAnimationReplicationBlob(in, *static_cast<AnimationComponent*>(component));
    };
    registry.Register(std::move(animationDesc));

    Engine::Net::ReplicationComponentDescriptor colliderDesc;
    colliderDesc.id = Engine::Net::kColliderReplicationComponent;
    colliderDesc.name = "Collider";
    colliderDesc.serialize = [](const void* component, Engine::Net::NetBuffer& out) {
        out = Engine::Net::SerializeColliderReplication(
            *static_cast<const ColliderComponent*>(component));
        return true;
    };
    colliderDesc.deserialize = [](const Engine::Net::NetBuffer& in, void* component) {
        return Engine::Net::DeserializeColliderReplication(in,
                                                            *static_cast<ColliderComponent*>(component));
    };
    registry.Register(std::move(colliderDesc));
}

} // namespace

ReplicationSystem::ReplicationSystem() {
    SetPriority(54);
    RegisterDefaultReplicators(m_registry);
    RegisterBuiltInBindings();
}

void ReplicationSystem::RegisterComponentBinding(ReplicationComponentBinding binding) {
    if (binding.componentId == Engine::Net::kInvalidReplicationComponent) {
        return;
    }
    m_bindings.push_back(std::move(binding));
}

void ReplicationSystem::RegisterBuiltInBindings() {
    RegisterComponentBinding(ReplicationComponentBinding{
        Engine::Net::kTransformReplicationComponent,
        [](EntityID entity, ECS::Registry& registry, Engine::Net::NetBuffer& out) {
            const TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
            if (!transform) {
                return false;
            }
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
        },
        [](EntityID entity, ECS::Registry& registry, const Engine::Net::NetBuffer& in) {
            TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
            if (!transform) {
                transform = &registry.AddComponent<TransformComponent>(entity);
            }
            Engine::Net::NetBuffer copy = in;
            copy.ResetRead();
            float scaleX = 1.0f;
            float scaleY = 1.0f;
            float scaleZ = 1.0f;
            // Position/rotation are networked; visual scale is owned by CharacterLoader /
            // remote spawn (server entities are unscaled). Applying scale=1 stomps ~0.09 mesh scale.
            const bool ok = copy.ReadFloat(transform->position.x) &&
                            copy.ReadFloat(transform->position.y) &&
                            copy.ReadFloat(transform->position.z) &&
                            copy.ReadFloat(transform->rotation.x) &&
                            copy.ReadFloat(transform->rotation.y) &&
                            copy.ReadFloat(transform->rotation.z) &&
                            copy.ReadFloat(scaleX) &&
                            copy.ReadFloat(scaleY) &&
                            copy.ReadFloat(scaleZ);
            (void)scaleX;
            (void)scaleY;
            (void)scaleZ;
            return ok;
        }});

    RegisterComponentBinding(ReplicationComponentBinding{
        Engine::Net::kAnimationReplicationComponent,
        [](EntityID entity, ECS::Registry& registry, Engine::Net::NetBuffer& out) {
            const AnimationComponent* anim = registry.GetComponent<AnimationComponent>(entity);
            if (!anim) {
                return false;
            }
            WriteAnimationReplicationBlob(*anim, out);
            return true;
        },
        [](EntityID entity, ECS::Registry& registry, const Engine::Net::NetBuffer& in) {
            AnimationComponent* anim = registry.GetComponent<AnimationComponent>(entity);
            if (!anim) {
                anim = &registry.AddComponent<AnimationComponent>(entity);
            }
            return ReadAnimationReplicationBlob(in, *anim);
        }});

    RegisterComponentBinding(ReplicationComponentBinding{
        Engine::Net::kColliderReplicationComponent,
        [](EntityID entity, ECS::Registry& registry, Engine::Net::NetBuffer& out) {
            const ColliderComponent* collider = registry.GetComponent<ColliderComponent>(entity);
            if (!collider) {
                return false;
            }
            out = Engine::Net::SerializeColliderReplication(*collider);
            return true;
        },
        [](EntityID entity, ECS::Registry& registry, const Engine::Net::NetBuffer& in) {
            ColliderComponent* collider = registry.GetComponent<ColliderComponent>(entity);
            if (!collider) {
                collider = &registry.AddComponent<ColliderComponent>(entity);
            }
            return Engine::Net::DeserializeColliderReplication(in, *collider);
        }});
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
    for (const ReplicationComponentBinding& binding : m_bindings) {
        Engine::Net::NetBuffer buffer;
        if (!binding.serialize || !binding.serialize(ecsEntity, registry, buffer)) {
            continue;
        }
        Engine::Net::ComponentSnapshot componentSnapshot;
        componentSnapshot.entityId = netEntityId;
        componentSnapshot.componentId = binding.componentId;
        componentSnapshot.tick = tick;
        componentSnapshot.data = buffer.Data();
        snapshot.components.push_back(std::move(componentSnapshot));
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
        for (const ReplicationComponentBinding& binding : m_bindings) {
            if (binding.componentId != componentSnapshot.componentId || !binding.deserialize) {
                continue;
            }
            binding.deserialize(ecsEntity, registry, Engine::Net::NetBuffer(componentSnapshot.data));
            break;
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
