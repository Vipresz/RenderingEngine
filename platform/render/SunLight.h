#pragma once

#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Render {

// Shared directional light. The atmosphere system writes it before the world draws.
// Travel points from the sun toward the ground. ToSun points back at the sun.
struct SunLight {
    static glm::vec3 Travel() { return travel; }
    static glm::vec3 ToSun() { return toSun; }
    static glm::vec3 Colour() { return colour; }
    static float Ambient() { return ambient; }
    static float Diffuse() { return diffuse; }

    static void Set(const glm::vec3& travelDir, const glm::vec3& towardSun, const glm::vec3& lightColour,
                    float ambientIntensity, float diffuseIntensity) {
        const float travelLen = glm::length(travelDir);
        travel = travelLen > 0.0001f ? travelDir / travelLen : glm::vec3(0.3f, -1.0f, 0.2f);
        const float sunLen = glm::length(towardSun);
        toSun = sunLen > 0.0001f ? towardSun / sunLen : -travel;
        if (glm::dot(toSun, travel) > 0.0f) {
            toSun = -travel;
        }
        colour = lightColour;
        ambient = ambientIntensity;
        diffuse = diffuseIntensity;
    }

private:
    inline static glm::vec3 travel{0.3f, -1.0f, 0.2f};
    inline static glm::vec3 toSun{-0.3f, 1.0f, -0.2f};
    inline static glm::vec3 colour{1.0f, 1.0f, 1.0f};
    inline static float ambient = 0.3f;
    inline static float diffuse = 0.7f;
};

} // namespace Render
} // namespace Platform
} // namespace Engine
