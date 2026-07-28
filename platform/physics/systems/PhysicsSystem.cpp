#include "physics/systems/PhysicsSystem.h"

#include "physics/components/ColliderComponent.h"
#include "render/components/TransformComponent.h"

namespace Engine {
namespace Physics {
namespace Systems {

using Engine::Platform::Render::Components::TransformComponent;

PhysicsSystem::PhysicsSystem(IPhysicsWorld* world) : m_world(world) {
    SetPriority(20);
}

void PhysicsSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    (void)tickNumber;
    if (!m_world) {
        return;
    }

    auto entities = registry.View<Components::ColliderComponent, TransformComponent>();
    const glm::vec3 gravity = m_world->GetGravity();

    for (EntityID entityId : entities) {
        auto* collider = registry.GetComponent<Components::ColliderComponent>(entityId);
        auto* transform = registry.GetComponent<TransformComponent>(entityId);
        if (!collider || !transform || collider->isStatic) {
            continue;
        }

        if (collider->useGravity) {
            collider->velocity += gravity * deltaTime;
        }

        glm::vec3 position = transform->position;
        const glm::vec3 delta = collider->velocity * deltaTime;
        if (m_world->ResolveMovement(position, delta)) {
            transform->position = position;
        } else {
            collider->velocity = glm::vec3(0.0f);
        }

        const float groundY = m_world->SampleTerrainHeight(position.x, position.z);
        if (position.y <= groundY + 0.01f) {
            position.y = groundY;
            transform->position.y = groundY;
            collider->velocity.y = 0.0f;
        }
    }
}

} // namespace Systems
} // namespace Physics
} // namespace Engine
