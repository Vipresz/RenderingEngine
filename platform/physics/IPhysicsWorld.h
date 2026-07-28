#pragma once

#include "physics/PhysicsTypes.h"

#include <vector>

namespace Engine {
namespace Physics {

// Physics abstraction — terrain sampling, gravity, simple AABB collision.
class IPhysicsWorld {
public:
    virtual ~IPhysicsWorld() = default;

    virtual void SetGravity(const glm::vec3& gravity) = 0;
    virtual glm::vec3 GetGravity() const = 0;

    virtual void SetTerrainSampler(TerrainHeightSampler sampler) = 0;
    virtual float SampleTerrainHeight(float worldX, float worldZ) const = 0;

    virtual void RegisterStaticCollider(const Aabb& bounds) = 0;
    virtual void ClearStaticColliders() = 0;

    virtual bool ResolveMovement(glm::vec3& inOutPosition, const glm::vec3& desiredDelta) const = 0;
    virtual bool Raycast(const glm::vec3& origin,
                         const glm::vec3& direction,
                         float maxDistance,
                         glm::vec3& outHit) const = 0;
};

} // namespace Physics
} // namespace Engine
