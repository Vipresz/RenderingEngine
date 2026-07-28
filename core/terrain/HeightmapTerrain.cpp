#include "core/terrain/HeightmapTerrain.h"

#include <algorithm>
#include <cmath>

namespace Engine {
namespace Terrain {

bool HeightmapTerrain::LoadFromNormalizedGrid(const std::vector<float>& normalizedHeights,
                                              int gridWidth,
                                              int gridHeight,
                                              float sizeX,
                                              float sizeZ,
                                              float heightScale) {
    if (gridWidth <= 0 || gridHeight <= 0 ||
        normalizedHeights.size() < static_cast<size_t>(gridWidth * gridHeight)) {
        return false;
    }

    m_heightGrid = normalizedHeights;
    m_gridW = gridWidth;
    m_gridH = gridHeight;
    m_sizeX = sizeX;
    m_sizeZ = sizeZ;
    m_heightScale = heightScale;
    return true;
}

float HeightmapTerrain::GetHeightAt(float worldX, float worldZ) const {
    if (m_heightGrid.empty() || m_gridW <= 0 || m_gridH <= 0) {
        return 0.0f;
    }

    const float stepX =
        (m_gridH > 1) ? m_sizeX / static_cast<float>(m_gridH - 1) : 0.0f;
    const float stepZ =
        (m_gridW > 1) ? m_sizeZ / static_cast<float>(m_gridW - 1) : 0.0f;
    if (stepX <= 0.0f || stepZ <= 0.0f) {
        return m_heightGrid[0] * m_heightScale;
    }

    const float fx = worldX / stepX;
    const float fz = worldZ / stepZ;
    if (fx < 0.0f || fx > static_cast<float>(m_gridH - 1) || fz < 0.0f ||
        fz > static_cast<float>(m_gridW - 1)) {
        return 0.0f;
    }

    int row0 = static_cast<int>(std::floor(fx));
    int col0 = static_cast<int>(std::floor(fz));
    row0 = std::max(0, std::min(row0, m_gridH - 1));
    col0 = std::max(0, std::min(col0, m_gridW - 1));
    const int row1 = std::min(row0 + 1, m_gridH - 1);
    const int col1 = std::min(col0 + 1, m_gridW - 1);

    const float tx = fx - static_cast<float>(row0);
    const float tz = fz - static_cast<float>(col0);

    const float h00 = m_heightGrid[static_cast<size_t>(row0 * m_gridW + col0)] * m_heightScale;
    const float h01 = m_heightGrid[static_cast<size_t>(row0 * m_gridW + col1)] * m_heightScale;
    const float h10 = m_heightGrid[static_cast<size_t>(row1 * m_gridW + col0)] * m_heightScale;
    const float h11 = m_heightGrid[static_cast<size_t>(row1 * m_gridW + col1)] * m_heightScale;

    const float h0 = h00 * (1.0f - tz) + h01 * tz;
    const float h1 = h10 * (1.0f - tz) + h11 * tz;
    return h0 * (1.0f - tx) + h1 * tx;
}

} // namespace Terrain
} // namespace Engine
