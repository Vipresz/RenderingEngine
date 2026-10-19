#pragma once

#include <GL/glew.h>

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

// Material component - stores material properties
struct MaterialComponent {
    GLfloat specularIntensity = 0.5f;
    GLfloat shininess = 32.0f;
    GLfloat opacity = 1.0f;
    
    MaterialComponent() = default;
    MaterialComponent(GLfloat sIntensity, GLfloat shine) 
        : specularIntensity(sIntensity), shininess(shine) {}
    MaterialComponent(GLfloat sIntensity, GLfloat shine, GLfloat alpha)
        : specularIntensity(sIntensity), shininess(shine), opacity(alpha) {}
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine



