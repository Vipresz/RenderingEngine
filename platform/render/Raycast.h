#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <cfloat>

namespace Engine {
namespace Platform {
namespace Render {

struct Raycast {
    // Möller–Trumbore ray/triangle test. Returns true if hit with 0 <= t.
    static bool IntersectTriangle(const glm::vec3& origin, const glm::vec3& dir,
                                const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                float& outT) {
        constexpr float kEpsilon = 1e-6f;
        const glm::vec3 edge1 = v1 - v0;
        const glm::vec3 edge2 = v2 - v0;
        const glm::vec3 pvec = glm::cross(dir, edge2);
        const float det = glm::dot(edge1, pvec);
        if (std::fabs(det) < kEpsilon) {
            return false;
        }

        const float invDet = 1.0f / det;
        const glm::vec3 tvec = origin - v0;
        const float u = glm::dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f) {
            return false;
        }

        const glm::vec3 qvec = glm::cross(tvec, edge1);
        const float v = glm::dot(dir, qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }

        const float t = glm::dot(edge2, qvec) * invDet;
        if (t < 0.0f) {
            return false;
        }

        outT = t;
        return true;
    }
};

} // namespace Render
} // namespace Platform
} // namespace Engine
