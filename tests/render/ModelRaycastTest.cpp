#include <gtest/gtest.h>

#include "render/Model.h"
#include "render/Window.h"

#include <glm/glm.hpp>

#include <fstream>
#include <filesystem>

using Engine::Platform::Render::Model;
using Engine::Platform::Window;

namespace {

void WriteTinyHeightmapBmp(const std::filesystem::path& path) {
    const uint8_t header[] = {
        0x42, 0x4D, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    const uint8_t pixels[] = {
        0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };

    std::ofstream output(path, std::ios::binary);
    ASSERT_TRUE(output.is_open());
    output.write(reinterpret_cast<const char*>(header), sizeof(header));
    output.write(reinterpret_cast<const char*>(pixels), sizeof(pixels));
}

} // namespace

class ModelRaycastEnvironment : public ::testing::Test {
protected:
    Window window{64, 64};

    void SetUp() override {
        ASSERT_EQ(window.Initialize(), 0);
    }
};

TEST_F(ModelRaycastEnvironment, HeightmapOnlyModelRaycastsWithoutCollisionTriangles) {
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "renderingengine_heightmap_raycast.bmp";
    WriteTinyHeightmapBmp(tempPath);

    Model terrain;
    ASSERT_TRUE(terrain.CreateFromHeightmap(tempPath.string(), 10.0f, 10.0f, 5.0f));
    EXPECT_TRUE(terrain.HasHeightmapTerrain());

    const glm::mat4 modelMatrix(1.0f);
    const glm::vec3 origin(5.0f, 10.0f, 5.0f);
    const glm::vec3 dir(0.0f, -1.0f, 0.0f);
    float distance = 0.0f;
    EXPECT_TRUE(terrain.RaycastWorld(modelMatrix, origin, dir, 20.0f, distance));
    EXPECT_GT(distance, 0.0f);
    EXPECT_LT(distance, 20.0f);

    std::filesystem::remove(tempPath);
}
