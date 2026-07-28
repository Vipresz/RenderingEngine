#pragma once

#include <string>
#include <vector>

namespace Engine {
namespace Terrain {

// Headless heightmap sampler (no OpenGL). Matches Model::GetHeightAt sampling logic.
class HeightmapTerrain {
public:
    bool LoadFromImage(const std::string& imagePath, float sizeX, float sizeZ, float heightScale = 10.0f);
    bool LoadFromNormalizedGrid(const std::vector<float>& normalizedHeights,
                                int gridWidth,
                                int gridHeight,
                                float sizeX,
                                float sizeZ,
                                float heightScale = 10.0f);
    bool IsLoaded() const { return !m_heightGrid.empty(); }
    float GetHeightAt(float worldX, float worldZ) const;

    float GetSizeX() const { return m_sizeX; }
    float GetSizeZ() const { return m_sizeZ; }
    float GetHeightScale() const { return m_heightScale; }

private:
    std::vector<float> m_heightGrid;
    int m_gridW = 0;
    int m_gridH = 0;
    float m_sizeX = 0.0f;
    float m_sizeZ = 0.0f;
    float m_heightScale = 10.0f;
};

} // namespace Terrain
} // namespace Engine
