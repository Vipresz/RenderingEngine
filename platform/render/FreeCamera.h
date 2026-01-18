#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cstddef>

struct GLFWwindow;

namespace Engine {
namespace Platform {
namespace Render {

struct Camera {
    std::string name;
    glm::vec3 position;
    glm::vec3 target;
    bool canMove;
    
    Camera(const std::string& n, const glm::vec3& pos, const glm::vec3& tgt, bool movable = false)
        : name(n), position(pos), target(tgt), canMove(movable) {}
};

class FreeCamera {
public:
    FreeCamera();
    ~FreeCamera();
    
    void Initialize();
    
    void AddCamera(const std::string& name, const glm::vec3& position, const glm::vec3& target, bool canMove);
    void InitializeDefault();
    
    void UpdateCamera(float deltaTime, GLFWwindow* window);
    void HandleMouseInput(bool& mouseRightDown, double& lastMouseX, double& lastMouseY);
    glm::mat4 GetViewMatrix() const;
    
    size_t GetActiveCameraIndex() const { return m_activeCameraIndex; }
    void SetActiveCameraIndex(size_t index);
    
    glm::vec3 GetPosition() const;
    glm::vec3 GetTarget() const;
    
    void SetMovementSpeed(float speed) { m_movementSpeed = speed; }
    float GetMovementSpeed() const { return m_movementSpeed; }
    
    static void CalculateYawPitchFromDirection(const glm::vec3& direction, float& yaw, float& pitch);
    
private:
    std::vector<Camera> m_cameras;
    size_t m_activeCameraIndex;
    glm::vec3 m_freeCameraPos;
    float m_freeCameraYaw;
    float m_freeCameraPitch;
    float m_movementSpeed;
    
    void UpdateFreeCamera(float deltaTime, GLFWwindow* window);
    glm::mat4 CalculateCameraView(const Camera& cam) const;
};

} // namespace Render
} // namespace Platform
} // namespace Engine

