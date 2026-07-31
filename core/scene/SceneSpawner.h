#pragma once

#include "core/ecs/Registry.h"
#include "core/scene/Prefab.h"
#include "core/scene/Scene.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {
namespace Scene {

struct SpawnedPrefab {
    std::vector<EntityID> entities;
    std::unordered_map<size_t, EntityID> nodeIndexToEntity;
};

struct SpawnedScene {
    std::unordered_map<SceneNodeId, EntityID> nodeToEntity;
};

// Spawns prefab/scene graph nodes as ECS entities with transforms.
class SceneSpawner {
public:
    static SpawnedPrefab InstantiatePrefab(ECS::Registry& registry,
                                           const Prefab& prefab,
                                           const glm::vec3& worldOrigin = glm::vec3(0.0f));

    static SpawnedScene InstantiateScene(ECS::Registry& registry, Scene& scene);

    static bool LoadAndInstantiateScene(ECS::Registry& registry, const std::string& jsonPath);
};

} // namespace Scene
} // namespace Engine
