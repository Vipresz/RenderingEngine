#pragma once

#include "physics/PhysicsTypes.h"

namespace Engine {
namespace Physics {
namespace Components {

struct ColliderComponent {
    Aabb localBounds{{ -0.5f, 0.0f, -0.5f }, { 0.5f, 2.0f, 0.5f }};
    bool isStatic = false;
    bool useGravity = true;
    glm::vec3 velocity{0.0f};
};

} // namespace Components
} // namespace Physics
} // namespace Engine
