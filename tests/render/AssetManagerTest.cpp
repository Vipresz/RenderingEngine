#include <gtest/gtest.h>

#include "render/AssetManager.h"

using Engine::Platform::Render::AssetManager;

TEST(AssetManagerTest, EmptyPathReturnsNull) {
    AssetManager& manager = AssetManager::Instance();
    manager.Clear();
    EXPECT_EQ(manager.LoadModel(""), nullptr);
    EXPECT_EQ(manager.ReloadModel(""), nullptr);
}

TEST(AssetManagerTest, EvictRemovesCachedEntry) {
    AssetManager& manager = AssetManager::Instance();
    manager.Clear();

    EXPECT_FALSE(manager.EvictModel("missing/model.fbx"));
    EXPECT_EQ(manager.GetCachedModelCount(), 0u);
}

TEST(AssetManagerTest, ReloadIncrementsGeneration) {
    AssetManager& manager = AssetManager::Instance();
    manager.Clear();

    EXPECT_EQ(manager.GetLoadGeneration("graphics/missing.fbx"), 0u);
    EXPECT_EQ(manager.LoadModel("graphics/missing.fbx"), nullptr);
    EXPECT_EQ(manager.GetLoadGeneration("graphics/missing.fbx"), 0u);

    manager.EvictModel("graphics/missing.fbx");
    EXPECT_EQ(manager.ReloadModel("graphics/missing.fbx"), nullptr);
    EXPECT_EQ(manager.GetLoadGeneration("graphics/missing.fbx"), 0u);
}
