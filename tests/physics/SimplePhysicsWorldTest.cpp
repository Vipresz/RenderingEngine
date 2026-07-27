#include <gtest/gtest.h>

#include "physics/SimplePhysicsWorld.h"

using Engine::Physics::Aabb;
using Engine::Physics::SimplePhysicsWorld;

TEST(SimplePhysicsWorldTest, TerrainSamplerSetsHeight) {
    SimplePhysicsWorld world;
    world.SetTerrainSampler([](float x, float z) {
        (void)x;
        (void)z;
        return 5.0f;
    });

    EXPECT_FLOAT_EQ(world.SampleTerrainHeight(0.0f, 0.0f), 5.0f);
}

TEST(SimplePhysicsWorldTest, ResolveMovementSnapsToTerrain) {
    SimplePhysicsWorld world;
    world.SetTerrainSampler([](float, float) { return 2.0f; });

    glm::vec3 position(0.0f, 0.0f, 0.0f);
    const glm::vec3 delta(1.0f, -1.0f, 0.0f);
    ASSERT_TRUE(world.ResolveMovement(position, delta));
    EXPECT_GE(position.y, 2.0f);
}

TEST(SimplePhysicsWorldTest, StaticColliderBlocksMovement) {
    SimplePhysicsWorld world;
    Aabb blocker;
    blocker.min = glm::vec3(-1.0f, -1.0f, -1.0f);
    blocker.max = glm::vec3(1.0f, 1.0f, 1.0f);
    world.RegisterStaticCollider(blocker);

    glm::vec3 position(0.0f, 0.0f, 0.0f);
    const glm::vec3 delta(0.0f, 0.0f, 0.0f);
    EXPECT_FALSE(world.ResolveMovement(position, delta));
}

TEST(SimplePhysicsWorldTest, RaycastHitsTerrain) {
    SimplePhysicsWorld world;
    world.SetTerrainSampler([](float, float) { return 0.0f; });

    glm::vec3 hit;
    const bool didHit = world.Raycast(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 20.0f, hit);
    EXPECT_TRUE(didHit);
    EXPECT_NEAR(hit.y, 0.0f, 0.5f);
}
