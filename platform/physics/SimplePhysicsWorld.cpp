#include "physics/SimplePhysicsWorld.h"

#include <algorithm>
#include <cmath>

namespace Engine {
namespace Physics {

bool Aabb::Contains(const glm::vec3& point) const {
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool Aabb::Intersects(const Aabb& other) const {
    return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y &&
           min.z <= other.max.z && max.z >= other.min.z;
}

float SimplePhysicsWorld::SampleTerrainHeight(float worldX, float worldZ) const {
    if (m_terrainSampler) {
        return m_terrainSampler(worldX, worldZ);
    }
    return 0.0f;
}

void SimplePhysicsWorld::RegisterStaticCollider(const Aabb& bounds) {
    m_staticColliders.push_back(bounds);
}

void SimplePhysicsWorld::ClearStaticColliders() {
    m_staticColliders.clear();
}

bool SimplePhysicsWorld::ResolveMovement(glm::vec3& inOutPosition, const glm::vec3& desiredDelta) const {
    glm::vec3 candidate = inOutPosition + desiredDelta;

    if (m_terrainSampler) {
        const float groundY = SampleTerrainHeight(candidate.x, candidate.z);
        candidate.y = std::max(candidate.y, groundY);
    }

    for (const Aabb& collider : m_staticColliders) {
        if (collider.Contains(candidate)) {
            return false;
        }
    }

    inOutPosition = candidate;
    return true;
}

bool SimplePhysicsWorld::Raycast(const glm::vec3& origin,
                               const glm::vec3& direction,
                               float maxDistance,
                               glm::vec3& outHit) const {
    const float length = glm::length(direction);
    if (length < 0.0001f || maxDistance <= 0.0f) {
        return false;
    }

    const glm::vec3 dir = direction / length;
    const int steps = static_cast<int>(std::max(1.0f, maxDistance * 4.0f));
    const float stepDistance = maxDistance / static_cast<float>(steps);

    glm::vec3 position = origin;
    for (int i = 0; i < steps; ++i) {
        position += dir * stepDistance;
        if (m_terrainSampler) {
            const float groundY = SampleTerrainHeight(position.x, position.z);
            if (position.y <= groundY) {
                outHit = glm::vec3(position.x, groundY, position.z);
                return true;
            }
        }
        for (const Aabb& collider : m_staticColliders) {
            if (collider.Contains(position)) {
                outHit = position;
                return true;
            }
        }
    }
    return false;
}

} // namespace Physics
} // namespace Engine
