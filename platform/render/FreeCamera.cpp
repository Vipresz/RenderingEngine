#include "FreeCamera.h"
#include "../Logger.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

using EngineLogging::Logger;

namespace Engine {
namespace Platform {
namespace Render {

FreeCamera::FreeCamera()
    : m_activeCameraIndex(0)
    , m_freeCameraPos(0.0f, 0.0f, 5.0f)
    , m_freeCameraYaw(0.0f)
    , m_freeCameraPitch(0.0f)
    , m_movementSpeed(5.0f)
{
}

FreeCamera::~FreeCamera() {
}

void FreeCamera::Initialize() {
    m_activeCameraIndex = 0;
}

void FreeCamera::AddCamera(const std::string& name, const glm::vec3& position, const glm::vec3& target, bool canMove) {
    m_cameras.emplace_back(name, position, target, canMove);
}

void FreeCamera::InitializeDefault() {
    m_cameras.clear();
    AddCamera("Free Camera", glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), true);
}

void FreeCamera::UpdateCamera(float deltaTime, GLFWwindow* window) {
    if (m_cameras.empty() || m_activeCameraIndex >= m_cameras.size()) {
        return;
    }
    
    if (m_cameras[m_activeCameraIndex].canMove && window) {
        UpdateFreeCamera(deltaTime, window);
    }
}

void FreeCamera::UpdateFreeCamera(float deltaTime, GLFWwindow* window) {
    if (deltaTime <= 0.0f || deltaTime > 1.0f) {
        return;
    }
    
    float moveSpeed = m_movementSpeed * deltaTime;
    
    float cosPitch = cosf(m_freeCameraPitch);
    float sinYaw = sinf(m_freeCameraYaw);
    float cosYaw = cosf(m_freeCameraYaw);
    
    glm::vec3 forward(cosPitch * sinYaw, cosPitch * cosYaw, 0.0f);
    float forwardLength = glm::length(forward);
    if (forwardLength > 0.0001f) {
        forward = forward / forwardLength;
    } else {
        forward = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    
    glm::vec3 right(cosYaw, -sinYaw, 0.0f);
    float rightLength = glm::length(right);
    if (rightLength > 0.0001f) {
        right = right / rightLength;
    } else {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        m_freeCameraPos += forward * moveSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        m_freeCameraPos -= forward * moveSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        m_freeCameraPos -= right * moveSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        m_freeCameraPos += right * moveSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        m_freeCameraPos.z += moveSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || 
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        m_freeCameraPos.z -= moveSpeed;
    }
}

void FreeCamera::HandleMouseInput(bool& mouseRightDown, double& lastMouseX, double& lastMouseY) {
    if (m_cameras.empty() || m_activeCameraIndex >= m_cameras.size() || !m_cameras[m_activeCameraIndex].canMove) {
        return;
    }
    
    // Mouse input is handled via GLFW callbacks, but we can track state here if needed
}

glm::mat4 FreeCamera::GetViewMatrix() const {
    if (m_cameras.empty() || m_activeCameraIndex >= m_cameras.size()) {
        return glm::mat4(1.0f);
    }
    
    const Camera& activeCam = m_cameras[m_activeCameraIndex];
    
    if (activeCam.canMove) {
        glm::vec3 forward(
            cosf(m_freeCameraPitch) * sinf(m_freeCameraYaw),
            cosf(m_freeCameraPitch) * cosf(m_freeCameraYaw),
            -sinf(m_freeCameraPitch)
        );
        
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
        glm::vec3 up = glm::cross(right, forward);
        if (glm::length(up) < 0.0001f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            up = glm::normalize(up);
        }
        
        return glm::lookAt(m_freeCameraPos, m_freeCameraPos + forward, up);
    } else {
        glm::vec3 dir = activeCam.target - activeCam.position;
        if (glm::length(dir) < 0.0001f) {
            dir = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        return glm::lookAt(activeCam.position, activeCam.target, glm::vec3(0.0f, 0.0f, 1.0f));
    }
}

void FreeCamera::SetActiveCameraIndex(size_t index) {
    if (index < m_cameras.size()) {
        m_activeCameraIndex = index;
        if (m_cameras[index].canMove) {
            m_freeCameraPos = m_cameras[index].position;
        }
    }
}

glm::vec3 FreeCamera::GetPosition() const {
    if (m_cameras.empty() || m_activeCameraIndex >= m_cameras.size()) {
        return glm::vec3(0.0f);
    }
    
    if (m_cameras[m_activeCameraIndex].canMove) {
        return m_freeCameraPos;
    }
    return m_cameras[m_activeCameraIndex].position;
}

glm::vec3 FreeCamera::GetTarget() const {
    if (m_cameras.empty() || m_activeCameraIndex >= m_cameras.size()) {
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }
    
    const Camera& activeCam = m_cameras[m_activeCameraIndex];
    if (activeCam.canMove) {
        glm::vec3 forward(
            cosf(m_freeCameraPitch) * sinf(m_freeCameraYaw),
            cosf(m_freeCameraPitch) * cosf(m_freeCameraYaw),
            -sinf(m_freeCameraPitch)
        );
        return m_freeCameraPos + forward;
    }
    return activeCam.target;
}

void FreeCamera::CalculateYawPitchFromDirection(const glm::vec3& direction, float& yaw, float& pitch) {
    yaw = atan2f(direction.x, direction.y);
    pitch = -asinf(std::max(-1.0f, std::min(1.0f, direction.z)));
}

} // namespace Render
} // namespace Platform
} // namespace Engine

