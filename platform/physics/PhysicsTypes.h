#pragma once

#include <glm/glm.hpp>
#include <functional>

namespace Engine {
namespace Physics {

using TerrainHeightSampler = std::function<float(float worldX, float worldZ)>;

struct Aabb {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    bool Contains(const glm::vec3& point) const;
    bool Intersects(const Aabb& other) const;
};

} // namespace Physics
} // namespace Engine
