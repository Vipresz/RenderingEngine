#include <gtest/gtest.h>

#include "core/scene/Scene.h"
#include "core/scene/SceneSerializer.h"

using Engine::Scene::Scene;
using Engine::Scene::SceneSerializer;

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

TEST(SceneSerializerTest, DeserializeSceneReturnsFalseForNow) {
    Scene scene;
    EXPECT_FALSE(SceneSerializer::DeserializeScene("{}", scene));
}
