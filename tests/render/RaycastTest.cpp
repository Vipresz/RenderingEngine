#include <gtest/gtest.h>

#include "render/Raycast.h"

#include <glm/glm.hpp>

using Engine::Platform::Render::Raycast;

TEST(RaycastTest, HitsFrontFacingTriangle) {
    const glm::vec3 origin(0.0f, 0.0f, 0.0f);
    const glm::vec3 dir(0.0f, 0.0f, 1.0f);
    const glm::vec3 v0(-1.0f, -1.0f, 1.0f);
    const glm::vec3 v1(1.0f, -1.0f, 1.0f);
    const glm::vec3 v2(0.0f, 1.0f, 1.0f);

    float t = 0.0f;
    ASSERT_TRUE(Raycast::IntersectTriangle(origin, dir, v0, v1, v2, t));
    EXPECT_NEAR(t, 1.0f, 1e-4f);
}

TEST(RaycastTest, MissesParallelRay) {
    const glm::vec3 origin(0.0f, 0.0f, 0.0f);
    const glm::vec3 dir(1.0f, 0.0f, 0.0f);
    const glm::vec3 v0(0.0f, -1.0f, 1.0f);
    const glm::vec3 v1(0.0f, 1.0f, 1.0f);
    const glm::vec3 v2(0.0f, 0.0f, 2.0f);

    float t = 0.0f;
    EXPECT_FALSE(Raycast::IntersectTriangle(origin, dir, v0, v1, v2, t));
}
