#include <gtest/gtest.h>

#include <filesystem>

TEST(ShadowShaderFilesTest, ShadowDepthShadersExist) {
    const std::filesystem::path repoRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::filesystem::path vert = repoRoot / "platform/render/shaders/shadow_depth.vert";
    const std::filesystem::path frag = repoRoot / "platform/render/shaders/shadow_depth.frag";
    EXPECT_TRUE(std::filesystem::exists(vert)) << vert.string();
    EXPECT_TRUE(std::filesystem::exists(frag)) << frag.string();
}
