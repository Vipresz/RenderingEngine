#pragma once

#include "core/Types.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Engine {
namespace Scene {

using SceneNodeId = uint32_t;
constexpr SceneNodeId kInvalidSceneNode = 0;

// Hierarchical scene graph node.
struct SceneNode {
    SceneNodeId id = kInvalidSceneNode;
    SceneNodeId parentId = kInvalidSceneNode;
    std::string name;
    glm::vec3 localPosition{0.0f};
    glm::vec3 localRotationDegrees{0.0f};
    glm::vec3 localScale{1.0f};
    EntityID entity = INVALID_ENTITY;
    std::vector<SceneNodeId> children;
    bool active = true;
};

} // namespace Scene
} // namespace Engine
