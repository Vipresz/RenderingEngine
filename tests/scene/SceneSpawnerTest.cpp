#include <gtest/gtest.h>

#include "core/ecs/Registry.h"
#include "core/scene/Prefab.h"
#include "core/scene/Scene.h"
#include "core/scene/SceneSpawner.h"
#include "physics/components/ColliderComponent.h"
#include "render/components/TransformComponent.h"

#include <glm/glm.hpp>

using Engine::ECS::Registry;
using Engine::Scene::Prefab;
using Engine::Scene::Scene;
using Engine::Scene::SceneNode;
using Engine::Scene::SceneSpawner;

TEST(SceneSpawnerTest, InstantiatePrefabCreatesEntitiesWithTransforms) {
    Registry registry;
    Prefab prefab;
    prefab.name = "Campfire";
    prefab.rootNodeIndex = 0;

    SceneNode root;
    root.name = "Root";
    root.localPosition = glm::vec3(1.0f, 0.0f, 2.0f);
    prefab.nodes.push_back(root);

    SceneNode child;
    child.name = "Flame";
    child.localPosition = glm::vec3(0.0f, 1.5f, 0.0f);
    prefab.nodes.push_back(child);

    const auto spawned = SceneSpawner::InstantiatePrefab(registry, prefab, glm::vec3(10.0f, 0.0f, 0.0f));
    ASSERT_EQ(spawned.entities.size(), 2u);
    ASSERT_EQ(spawned.nodeIndexToEntity.size(), 2u);

    const auto* rootTransform =
        registry.GetComponent<Engine::Platform::Render::Components::TransformComponent>(
            spawned.nodeIndexToEntity.at(0));
    ASSERT_NE(rootTransform, nullptr);
    EXPECT_FLOAT_EQ(rootTransform->position.x, 11.0f);
    EXPECT_FLOAT_EQ(rootTransform->position.z, 2.0f);

    const auto* childTransform =
        registry.GetComponent<Engine::Platform::Render::Components::TransformComponent>(
            spawned.nodeIndexToEntity.at(1));
    ASSERT_NE(childTransform, nullptr);
    EXPECT_FLOAT_EQ(childTransform->position.y, 1.5f);
}

TEST(SceneSpawnerTest, InstantiateSceneCreatesEntitiesForActiveNodes) {
    Registry registry;
    Scene scene;
    const auto root = scene.CreateNode("WorldRoot");
    const auto marker = scene.CreateNode("SpawnMarker", root);
    scene.GetNode(marker)->localPosition = glm::vec3(0.0f, 3.0f, 0.0f);

    const auto spawned = SceneSpawner::InstantiateScene(registry, scene);
    EXPECT_EQ(spawned.nodeToEntity.size(), 2u);

    const auto* markerTransform =
        registry.GetComponent<Engine::Platform::Render::Components::TransformComponent>(
            spawned.nodeToEntity.at(marker));
    ASSERT_NE(markerTransform, nullptr);
    EXPECT_FLOAT_EQ(markerTransform->position.y, 3.0f);
}
