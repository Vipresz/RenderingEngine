#include "render/Frustum.h"

#include <algorithm>
#include <cmath>

namespace Engine {
namespace Platform {
namespace Render {

Frustum ExtractFrustum(const glm::mat4& viewProjection) {
    Frustum frustum;
    const glm::mat4& m = viewProjection;

    frustum.planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]);
    frustum.planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]);
    frustum.planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]);
    frustum.planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]);
    frustum.planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]);
    frustum.planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);

    for (glm::vec4& plane : frustum.planes) {
        const float length = glm::length(glm::vec3(plane));
        if (length > 1e-6f) {
            plane /= length;
        }
    }
    return frustum;
}

bool IntersectsAABB(const Frustum& frustum,
                    const glm::vec3& boundsMin,
                    const glm::vec3& boundsMax) {
    for (const glm::vec4& plane : frustum.planes) {
        const glm::vec3 normal(plane);
        const glm::vec3 positiveVertex(
            plane.x >= 0.0f ? boundsMax.x : boundsMin.x,
            plane.y >= 0.0f ? boundsMax.y : boundsMin.y,
            plane.z >= 0.0f ? boundsMax.z : boundsMin.z);
        if (glm::dot(normal, positiveVertex) + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace Render
} // namespace Platform
} // namespace Engine
