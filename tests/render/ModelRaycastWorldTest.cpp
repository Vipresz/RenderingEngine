#include <gtest/gtest.h>

#include "render/Model.h"

#include <glm/glm.hpp>

using Engine::Platform::Render::Model;

TEST(ModelRaycastWorldTest, RejectsEmptyModelWithoutCollisionOrHeightmap) {
    Model emptyModel;

    const glm::mat4 modelMatrix(1.0f);
    const glm::vec3 origin(0.0f, 5.0f, 0.0f);
    const glm::vec3 dir(0.0f, -1.0f, 0.0f);
    float distance = 0.0f;

    EXPECT_FALSE(emptyModel.HasHeightmapTerrain());
    EXPECT_FALSE(emptyModel.RaycastWorld(modelMatrix, origin, dir, 20.0f, distance));
}

TEST(ModelRaycastWorldTest, RejectsNonPositiveMaxDistance) {
    Model model;
    float distance = 0.0f;
    EXPECT_FALSE(model.RaycastWorld(glm::mat4(1.0f), glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 0.0f, distance));
    EXPECT_FALSE(model.RaycastWorld(glm::mat4(1.0f), glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), -1.0f, distance));
}
