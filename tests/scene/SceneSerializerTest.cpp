#include <gtest/gtest.h>

#include "core/scene/Scene.h"
#include "core/scene/SceneNode.h"
#include "core/scene/SceneSerializer.h"
#include "core/scene/Prefab.h"

#include <glm/glm.hpp>

using Engine::Scene::Scene;
using Engine::Scene::SceneSerializer;
using Engine::Scene::Prefab;
using Engine::Scene::SceneNode;

TEST(SceneSerializerTest, SerializeSceneContainsNodeData) {
    Scene scene;
    const auto root = scene.CreateNode("PlayerRoot");
    auto* node = scene.GetNode(root);
    ASSERT_NE(node, nullptr);
    node->localPosition = glm::vec3(1.0f, 2.0f, 3.0f);

    const std::string json = SceneSerializer::SerializeScene(scene);
    EXPECT_NE(json.find("PlayerRoot"), std::string::npos);
    EXPECT_NE(json.find("\"position\":[1,2,3]"), std::string::npos);
}

TEST(SceneSerializerTest, DeserializeSceneRoundTrip) {
    Scene scene;
    const auto root = scene.CreateNode("PlayerRoot");
    auto* node = scene.GetNode(root);
    ASSERT_NE(node, nullptr);
    node->localPosition = glm::vec3(1.0f, 2.0f, 3.0f);
    node->localRotationDegrees = glm::vec3(4.0f, 5.0f, 6.0f);

    const std::string json = SceneSerializer::SerializeScene(scene);
    Scene loaded;
    ASSERT_TRUE(SceneSerializer::DeserializeScene(json, loaded));

    const SceneNode* loadedNode = loaded.GetNode(root);
    ASSERT_NE(loadedNode, nullptr);
    EXPECT_EQ(loadedNode->name, "PlayerRoot");
    EXPECT_FLOAT_EQ(loadedNode->localPosition.z, 3.0f);
    EXPECT_FLOAT_EQ(loadedNode->localRotationDegrees.x, 4.0f);
}

TEST(SceneSerializerTest, DeserializePrefabRoundTrip) {
    Prefab prefab;
    prefab.name = "Tree";
    prefab.rootNodeIndex = 0;
    SceneNode node;
    node.name = "Trunk";
    node.localPosition = glm::vec3(0.0f, 1.0f, 0.0f);
    prefab.nodes.push_back(node);

    const std::string json = SceneSerializer::SerializePrefab(prefab);
    Prefab loaded;
    ASSERT_TRUE(SceneSerializer::DeserializePrefab(json, loaded));
    EXPECT_EQ(loaded.name, "Tree");
    ASSERT_EQ(loaded.nodes.size(), 1u);
    EXPECT_EQ(loaded.nodes[0].name, "Trunk");
    EXPECT_FLOAT_EQ(loaded.nodes[0].localPosition.y, 1.0f);
}
