#pragma once

#include <GL/glew.h>

#include <string>

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

// Four blended PBR layers plus region overlay weights.
// Texture ids are owned by TerrainMaterialLibrary.
struct TerrainMaterialComponent {
    bool active = false;
    std::string regionId;

    GLuint albedo[4] = {0, 0, 0, 0};
    GLuint normalMap[4] = {0, 0, 0, 0};
    GLuint orm[4] = {0, 0, 0, 0};
    GLuint materialMask = 0;
    GLuint flowMask = 0;
    float useMasks = 0.0f;

    float tint[3] = {1.0f, 1.0f, 1.0f};
    float roughnessBias = 0.0f;
    float uvScale = 0.16f;
    float terrainHeight = 10.0f;
    float slopeRock = 0.45f;
    float heightCover = 0.0f;
    float coverAmount = 0.75f;
    float accentAmount = 0.3f;
    float moss = 0.0f;
    float snow = 0.0f;
    float ash = 0.0f;
    float dust = 0.0f;
    float wet = 0.0f;
    float cracks = 0.0f;
    float exoticAmount = 0.0f;
    float exoticPulse = 1.0f;
    float exoticColor[3] = {0.4f, 0.9f, 0.55f};
    float paintAmount = 0.0f;
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine
