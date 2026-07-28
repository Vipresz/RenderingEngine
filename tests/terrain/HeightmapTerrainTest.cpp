#include <gtest/gtest.h>

#include "core/terrain/HeightmapTerrain.h"

#include <cmath>
#include <vector>

using Engine::Terrain::HeightmapTerrain;

TEST(HeightmapTerrainTest, SamplesBilinearHeightFromGrid) {
    const std::vector<float> grid = {
        0.0f, 0.5f,
        1.0f, 1.0f,
    };

    HeightmapTerrain terrain;
    ASSERT_TRUE(terrain.LoadFromNormalizedGrid(grid, 2, 2, 10.0f, 10.0f, 8.0f));

    const float corner = terrain.GetHeightAt(0.0f, 0.0f);
    const float center = terrain.GetHeightAt(5.0f, 5.0f);
    EXPECT_FLOAT_EQ(corner, 0.0f);
    EXPECT_GT(center, corner);
    EXPECT_LE(center, 8.0f);
    EXPECT_FALSE(std::isnan(center));
}
