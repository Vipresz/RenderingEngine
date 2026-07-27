#pragma once

#include "physics/IPhysicsWorld.h"

#include <vector>

namespace Engine {
namespace Physics {

class SimplePhysicsWorld : public IPhysicsWorld {
public:
    void SetGravity(const glm::vec3& gravity) override { m_gravity = gravity; }
    glm::vec3 GetGravity() const override { return m_gravity; }

    void SetTerrainSampler(TerrainHeightSampler sampler) override { m_terrainSampler = std::move(sampler); }
    float SampleTerrainHeight(float worldX, float worldZ) const override;

    void RegisterStaticCollider(const Aabb& bounds) override;
    void ClearStaticColliders() override;

    bool ResolveMovement(glm::vec3& inOutPosition, const glm::vec3& desiredDelta) const override;
    bool Raycast(const glm::vec3& origin,
                 const glm::vec3& direction,
                 float maxDistance,
                 glm::vec3& outHit) const override;

private:
    glm::vec3 m_gravity{0.0f, -9.81f, 0.0f};
    TerrainHeightSampler m_terrainSampler;
    std::vector<Aabb> m_staticColliders;
};

} // namespace Physics
} // namespace Engine
