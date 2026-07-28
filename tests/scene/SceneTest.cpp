#include <gtest/gtest.h>

#include "core/scene/Scene.h"

using Engine::Scene::Scene;
using Engine::Scene::kInvalidSceneNode;

TEST(SceneTest, CreateHierarchyAndWorldMatrix) {
    Scene scene;
    const auto root = scene.CreateNode("Root");
    const auto child = scene.CreateNode("Child", root);

    auto* childNode = scene.GetNode(child);
    ASSERT_NE(childNode, nullptr);
    childNode->localPosition = glm::vec3(5.0f, 0.0f, 0.0f);

    const glm::mat4 world = scene.ComputeWorldMatrix(child);
    EXPECT_FLOAT_EQ(world[3][0], 5.0f);
}

TEST(SceneTest, DestroyNodeRemovesDescendants) {
    Scene scene;
    const auto root = scene.CreateNode("Root");
    const auto child = scene.CreateNode("Child", root);

    scene.DestroyNode(root);
    EXPECT_EQ(scene.GetNode(root), nullptr);
    EXPECT_EQ(scene.GetNode(child), nullptr);
}

TEST(SceneTest, RootIsFirstTopLevelNode) {
    Scene scene;
    const auto root = scene.CreateNode("Root");
    EXPECT_EQ(scene.GetRoot(), root);
    EXPECT_NE(root, kInvalidSceneNode);
}
