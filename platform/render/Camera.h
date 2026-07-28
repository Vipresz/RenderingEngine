#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include "render/ICameraSource.h"

namespace Engine {
namespace Platform {
namespace Render {

class Camera : public ICameraSource {
public:
    Camera();
    Camera(glm::vec3 startPosition, glm::vec3 startUp, 
           GLfloat startYaw, GLfloat startPitch, 
           GLfloat startMoveSpeed, GLfloat startTurnSpeed);
    
    void keyControl(bool* keys, GLfloat deltaTime);
    void OnMouseMove(float deltaX, float deltaY) override;
    glm::mat4 GetViewMatrix() const override;
    glm::vec3 GetEyePosition() const override;
    bool GetMovementBasis(glm::vec3& outForward, glm::vec3& outRight) const override;

    // Legacy API
    void mouseControl(GLfloat xChange, GLfloat yChange);
    glm::vec3 getCameraPosition();
    glm::vec3 getCameraDirection();
    glm::mat4 calculateViewMatrix();
    
    ~Camera();

private:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    
    GLfloat yaw;
    GLfloat pitch;
    
    GLfloat moveSpeed;
    GLfloat turnSpeed;
    
    void update();
};

} // namespace Render
} // namespace Platform
} // namespace Engine



