#pragma once

#include "core/System.h"
#include "core/ecs/Registry.h"
#include "physics/IPhysicsWorld.h"

#include <memory>

namespace Engine {
namespace Physics {
namespace Systems {

class PhysicsSystem : public ECS::ISystem {
public:
    explicit PhysicsSystem(IPhysicsWorld* world);

    void SetWorld(IPhysicsWorld* world) { m_world = world; }
    IPhysicsWorld* GetWorld() const { return m_world; }

    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;

private:
    IPhysicsWorld* m_world = nullptr;
};

} // namespace Systems
} // namespace Physics
} // namespace Engine
