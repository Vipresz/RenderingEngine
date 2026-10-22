#include "core/terrain/HeightmapTerrain.h"

#include "Logger.h"

#include "stb_image.h"

#include <algorithm>

namespace Engine {
namespace Terrain {

bool HeightmapTerrain::LoadFromImage(const std::string& imagePath, float sizeX, float sizeZ, float heightScale) {
    m_heightGrid.clear();
    m_gridW = 0;
    m_gridH = 0;

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 3);
    if (!data) {
        LOG_ERROR_FMT("HeightmapTerrain", "Failed to load heightmap: %s", imagePath.c_str());
        return false;
    }

    const int gridW = width;
    const int gridH = height;
    m_heightGrid.resize(static_cast<size_t>(gridW * gridH), 0.0f);

    float lumMin = 1.0f;
    float lumMax = 0.0f;
    for (int row = 0; row < gridH; ++row) {
        for (int col = 0; col < gridW; ++col) {
            const int idx = (row * gridW + col) * 3;
            const float r = static_cast<float>(data[idx + 0]) / 255.0f;
            const float g = static_cast<float>(data[idx + 1]) / 255.0f;
            const float b = static_cast<float>(data[idx + 2]) / 255.0f;
            const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
            lumMin = std::min(lumMin, lum);
            lumMax = std::max(lumMax, lum);
            m_heightGrid[static_cast<size_t>(row * gridW + col)] = lum;
        }
    }
    stbi_image_free(data);

    const float lumRange = lumMax - lumMin;
    if (lumRange > 0.0001f) {
        for (float& h : m_heightGrid) {
            h = (h - lumMin) / lumRange;
        }
    }

    m_gridW = gridW;
    m_gridH = gridH;
    m_sizeX = sizeX;
    m_sizeZ = sizeZ;
    m_heightScale = heightScale;

    LOG_INFO_FMT("HeightmapTerrain", "Loaded %s (%dx%d) span %.1fx%.1f scale %.1f",
                 imagePath.c_str(),
                 gridW,
                 gridH,
                 sizeX,
                 sizeZ,
                 heightScale);
    return true;
}

} // namespace Terrain
} // namespace Engine
