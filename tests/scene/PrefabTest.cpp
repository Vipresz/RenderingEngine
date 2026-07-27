#include <gtest/gtest.h>

#include "core/scene/Prefab.h"

using Engine::Scene::Prefab;
using Engine::Scene::PrefabLibrary;
using Engine::Scene::SceneNode;

TEST(PrefabTest, RegisterAndFindPrefab) {
    PrefabLibrary library;

    Prefab tree;
    tree.name = "tree";
    SceneNode node;
    node.name = "Trunk";
    tree.nodes.push_back(node);
    tree.rootNodeIndex = 0;

    library.Register("tree", tree);

    const Prefab* found = library.Find("tree");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "tree");
    ASSERT_EQ(found->nodes.size(), 1u);
    EXPECT_EQ(found->nodes[0].name, "Trunk");
}

TEST(PrefabTest, MissingPrefabReturnsNull) {
    PrefabLibrary library;
    EXPECT_EQ(library.Find("missing"), nullptr);
}
