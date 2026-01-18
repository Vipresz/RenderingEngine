#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {

class Shader;

// Grid renderer for drawing coordinate grids in 3D scenes
class GridRenderer {
public:
    GridRenderer();
    ~GridRenderer();
    
    // Initialize grid renderer (grid buffers, shaders)
    bool Initialize();
    
    // Cleanup
    void Cleanup();
    
    // Grid rendering
    void RenderGrid(const glm::mat4& projection, const glm::mat4& view);
    
    // Grid configuration
    void SetGridVisible(bool visible) { m_gridVisible = visible; }
    bool IsGridVisible() const { return m_gridVisible; }
    
    void SetGridSpacing(float spacing) { m_gridSpacing = spacing; }
    float GetGridSpacing() const { return m_gridSpacing; }
    
    void SetGridSize(float size) { m_gridSize = size; }
    float GetGridSize() const { return m_gridSize; }
    
    void SetGridCellCount(int cellCount) { m_gridCellCount = cellCount; }
    int GetGridCellCount() const { return m_gridCellCount; }
    
    void SetGridColor(const glm::vec3& color) { m_gridColor = color; }
    glm::vec3 GetGridColor() const { return m_gridColor; }
    
    // Access to shader (needed for initialization)
    Shader* GetGridShader() { return m_gridShader; }
    
private:
    // Grid rendering
    Shader* m_gridShader;
    GLuint m_gridVAO;
    GLuint m_gridVBO;
    bool m_buffersInitialized;
    bool m_gridVisible;
    
    // Grid configuration
    float m_gridSpacing;
    float m_gridSize;
    int m_gridCellCount;
    glm::vec3 m_gridColor;
    
    // Initialize OpenGL buffers for grid
    bool InitializeBuffers();
    
    // Create grid shader
    bool CreateGridShader();
};

} // namespace Render
} // namespace Platform
} // namespace Engine

