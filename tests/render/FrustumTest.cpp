#include <gtest/gtest.h>

#include "render/Frustum.h"

#include <glm/gtc/matrix_transform.hpp>

using Engine::Platform::Render::ExtractFrustum;
using Engine::Platform::Render::Frustum;
using Engine::Platform::Render::IntersectsAABB;

TEST(FrustumTest, ContainsOriginCubeInDefaultView) {
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    const Frustum frustum = ExtractFrustum(projection * view);

    EXPECT_TRUE(IntersectsAABB(frustum, glm::vec3(-1.0f), glm::vec3(1.0f)));
}

TEST(FrustumTest, RejectsCubeFarBehindCamera) {
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    const Frustum frustum = ExtractFrustum(projection * view);

    EXPECT_FALSE(IntersectsAABB(frustum, glm::vec3(0.0f, 0.0f, 20.0f), glm::vec3(2.0f, 2.0f, 22.0f)));
}

TEST(FrustumTest, RejectsCubeFarToTheSide) {
    const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    const Frustum frustum = ExtractFrustum(projection * view);

    EXPECT_FALSE(IntersectsAABB(frustum, glm::vec3(50.0f, -1.0f, -1.0f), glm::vec3(52.0f, 1.0f, 1.0f)));
}
