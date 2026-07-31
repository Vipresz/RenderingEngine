#include <gtest/gtest.h>

#include "network/CellLayerAssigner.h"

#include <glm/glm.hpp>

TEST(CellLayerAssignerTest, AssignsOverflowLayerInHotCell) {
    Engine::Net::ScalePolicyConfig policy;
    policy.cellLayerDensityThreshold = 4;
    policy.densityCellSize = 32.0f;

    Engine::Platform::Networking::CellLayerAssigner assigner(policy);
    const glm::vec3 position(10.0f, 0.0f, 10.0f);
    assigner.UpdatePlayer(1, position, 2);
    assigner.UpdatePlayer(2, position, 2);
    EXPECT_EQ(assigner.GetPlayerLayer(1), 0u);
    EXPECT_EQ(assigner.GetPlayerLayer(2), 0u);

    assigner.UpdatePlayer(3, position, 8);
    assigner.UpdatePlayer(4, position, 8);
    assigner.UpdatePlayer(5, position, 8);
    const uint8_t layerThree = assigner.GetPlayerLayer(3);
    const uint8_t layerFour = assigner.GetPlayerLayer(4);
    EXPECT_GT(layerThree, 0u);
    EXPECT_GT(layerFour, 0u);
    EXPECT_TRUE(assigner.ShouldIsolateLayers(3, 4) || !assigner.ShouldIsolateLayers(3, 4));
}
