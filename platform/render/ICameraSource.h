#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Engine {
namespace Platform {
namespace Render {

// Pluggable camera interface — RenderSystem, InputSystem, and CameraSystem use this.
// Implement in-engine (FollowCamera, Camera) or in game code (src/).
class ICameraSource {
public:
    virtual ~ICameraSource() = default;

    // Called each frame after gameplay input (CameraSystem, priority 10)
    virtual void Update(float deltaTime) { (void)deltaTime; }

    // Input hooks (called from InputSystem when game input is not blocked)
    virtual void OnMouseMove(float deltaX, float deltaY) {
        (void)deltaX;
        (void)deltaY;
    }
    virtual void OnScroll(float deltaY) { (void)deltaY; }

    // Rendering
    virtual glm::mat4 GetViewMatrix() const = 0;
    virtual glm::vec3 GetEyePosition() const = 0;

    // Optional — for camera-relative character movement (WASD)
    virtual bool GetMovementBasis(glm::vec3& outForward, glm::vec3& outRight) const {
        (void)outForward;
        (void)outRight;
        return false;
    }

    // Horizontal facing angle (degrees) matching TransformComponent::SetYaw convention
    virtual float GetPlanarFacingYawDegrees() const {
        glm::vec3 forward(0.0f);
        glm::vec3 right(0.0f);
        if (!GetMovementBasis(forward, right)) {
            return 0.0f;
        }
        forward.y = 0.0f;
        if (glm::length(forward) < 0.0001f) {
            return 0.0f;
        }
        forward = glm::normalize(forward);
        return glm::degrees(std::atan2(-forward.x, -forward.z));
    }
};

} // namespace Render
} // namespace Platform
} // namespace Engine
