#pragma once

#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Render {

struct Frustum {
    glm::vec4 planes[6]{};
};

Frustum ExtractFrustum(const glm::mat4& viewProjection);

bool IntersectsAABB(const Frustum& frustum,
                    const glm::vec3& boundsMin,
                    const glm::vec3& boundsMax);

} // namespace Render
} // namespace Platform
} // namespace Engine
