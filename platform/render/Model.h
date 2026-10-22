#pragma once

#include <vector>
#include <string>
#include <memory>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>

#include "render/Mesh.h"
#include "render/Texture.h"

namespace Engine {
namespace Platform {
namespace Render {

class Skeleton;

class Model {
public:
    Model();
    
    void LoadModel(const std::string& fileName);
    /// Build GPU meshes from an already-imported Assimp scene (no second disk read).
    /// @param sourcePath Original/resolved path used for texture directory resolution.
    bool LoadModelFromScene(const aiScene* scene, const std::string& sourcePath);
    void RenderModel();
    void RenderModelPerMeshTexture(GLint useTextureUniformLoc);
    void ClearModel();
    
    // Set base directory for resolving texture paths
    void SetTextureBaseDir(const std::string& baseDir) { m_textureBaseDir = baseDir; }
    
    // Get mesh count
    size_t GetMeshCount() const { return m_meshList.size(); }
    
    // Get model bounding box/extents (for camera positioning, etc.)
    // Returns: minX, maxX, minY, maxY, minZ, maxZ
    void GetBounds(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const;
    
    // Get the top-most Y coordinate (head height) of the model
    float GetTopY() const { return m_maxY; }
    
    // Get the bottom-most Y coordinate (base height) of the model
    float GetBottomY() const { return m_minY; }
    
    // Get the height of the model (top - bottom)
    float GetHeight() const { return m_maxY - m_minY; }
    
    /// Largest axis extent used for uniform "fit to target size" scaling (same length unit as
    /// scene grid and ui_defaults objects.size_default). Skips thin plates, then removes
    /// outlier mesh extents so a huge floor does not shrink the main subject.
    float GetUniformScaleReferenceExtent() const;
    
    // Create a simple plane programmatically (for ground, etc.)
    // sizeX, sizeZ: dimensions of the plane
    void CreatePlane(float sizeX, float sizeZ);
    
    // Create a sphere programmatically
    // radius: sphere radius
    // segments: number of segments (more = smoother, default 32)
    void CreateSphere(float radius, unsigned int segments = 32);
    
    // Create a grid programmatically (ground with grid lines)
    // sizeX, sizeZ: dimensions of the grid
    // divisions: number of grid divisions (default 10)
    void CreateGrid(float sizeX, float sizeZ, unsigned int divisions = 10);
    
    // Create terrain from a heightmap image (PNG, etc.)
    // imagePath: path to heightmap image (pixel luminance = height, dark=low, light=high)
    // sizeX, sizeZ: world dimensions (e.g. 100 x 100 units)
    // heightScale: scale factor for height values (default 10.0)
    // Mapping: image top-left (0,0) at world (0,0); Z axis along pixel rows (columns)
    // Returns false if image failed to load
    bool CreateFromHeightmap(const std::string& imagePath, float sizeX, float sizeZ, float heightScale = 10.0f);
    
    // Sample terrain height at world position (X, Z). Only valid for heightmap terrain.
    // Returns height in world units, or 0.0f if not a heightmap or out of bounds.
    float GetHeightAt(float worldX, float worldZ) const;

    bool HasHeightmapTerrain() const;
    float GetTerrainWorldSizeX() const { return m_terrainSizeX; }
    float GetTerrainWorldSizeZ() const { return m_terrainSizeZ; }

    // Camera / physics queries (model-local triangle soup built at load time).
    bool RaycastWorld(const glm::mat4& modelMatrix, const glm::vec3& origin, const glm::vec3& dir,
                      float maxDistance, float& outDistance) const;
    
    // Get bone name by mesh bone index (for bone remapping)
    // Returns empty string if bone index is invalid
    std::string GetBoneNameByIndex(unsigned int meshBoneIndex) const;
    
    // Get the number of bones in the model (max mesh bone index + 1)
    size_t GetBoneCount() const { return m_boneNames.size(); }

    /// Remap mesh-local Assimp bone indices to skeleton bone IDs (call after skeleton load).
    void RemapBoneIndicesToSkeleton(const Skeleton& skeleton);
    
    // Update vertex positions for animation (applies to all meshes)
    // vertexData: array of vertex data (position, UV, normal) per mesh
    // meshVertexCounts: number of vertices per mesh
    void UpdateVertexPositions(std::vector<std::vector<GLfloat>>& vertexData, 
                               const std::vector<unsigned int>& meshVertexCounts);
    
    // Apply rainbow color gradient along a mesh-local axis (0=X, 1=Y, 2=Z).
    // FBX bind pose is Y-up, so height must use axis 1 or the mesh looks flat.
    void ApplyRainbowColors(float minVal, float maxVal, int axis = 2);
    
    /// Bind-pose / rest vertices in mesh space (before the scene model matrix).
    void CopyBindPosePositions(std::vector<glm::vec3>& out) const;
    /// Texture coordinates in the same vertex order as CopyBindPosePositions.
    void CopyBindPoseTexcoords(std::vector<glm::vec2>& out) const;
    /// Bind-pose vertices skinned by \p boneMatrices (same order as the skeleton upload).
    void CopySkinnedPositions(const std::vector<glm::mat4>& boneMatrices, std::vector<glm::vec3>& out) const;
    /// Resolved diffuse image, empty when the material has no file texture.
    const std::string& DiffuseTexturePath() const { return m_diffuseTexturePath; }
    /// Repeat-wrap sample. v = 0 is the bottom of the image.
    bool SampleDiffuse(float u, float v, glm::vec3& rgb) const;

    // Check if model has any valid (non-null) textures
    bool HasTextures() const { 
        for (const auto* tex : m_textureList) {
            if (tex) return true;
        }
        return false;
    }
    
    ~Model();

private:
    void LoadNode(aiNode* node, const aiScene* scene);
    void LoadMesh(aiMesh* mesh, const aiScene* scene);
    void LoadMaterials(const aiScene* scene);
    
    std::string ResolveTexturePath(const std::string& texturePath);
    
    std::vector<Mesh*> m_meshList;
    std::vector<Texture*> m_textureList;
    std::vector<unsigned int> m_meshToTex;
    std::string m_textureBaseDir;
    std::string m_modelDirectory;
    
    // Bounding box extents (for camera positioning)
    float m_minX, m_maxX;
    float m_minY, m_maxY;
    float m_minZ, m_maxZ;
    bool m_boundsCalculated;
    
    // Bone name storage: mesh bone index → bone name
    // Used for remapping bone matrices to match skeleton bone IDs
    std::vector<std::string> m_boneNames; // Index = mesh bone index, value = bone name
    
    // Store vertex positions for color application
    std::vector<std::vector<glm::vec3>> m_vertexPositions; // Per-mesh vertex positions
    std::vector<std::vector<glm::vec2>> m_vertexTexcoords;
    std::string m_diffuseTexturePath;
    mutable std::vector<unsigned char> m_diffuseRgba;
    mutable int m_diffuseW = 0;
    mutable int m_diffuseH = 0;
    bool EnsureDiffusePixels() const;

    struct SkinnedMeshBoneNames {
        size_t meshIndex = 0;
        unsigned int vertexCount = 0;
        std::vector<std::string> names; // mesh bone index → bone name
    };
    std::vector<SkinnedMeshBoneNames> m_skinnedMeshBoneNames;
    bool m_skipMeshesWithoutBones = false;
    
    void RecalculateBoundsFromLoadedMeshes();

    // Heightmap terrain data (for GetHeightAt sampling)
    std::vector<float> m_heightGrid;
    int m_heightGridW = 0, m_heightGridH = 0;
    float m_terrainSizeX = 0.0f, m_terrainSizeZ = 0.0f, m_terrainHeightScale = 0.0f;

    std::vector<glm::vec3> m_collisionTriangles;

    void AppendCollisionTriangles(const GLfloat* vertices, const unsigned int* indices,
                                  unsigned int numVertices, unsigned int numIndices);
    bool RaycastLocal(const glm::vec3& origin, const glm::vec3& dir, float maxDistance,
                      float& outDistance) const;
    bool RaycastHeightmap(const glm::vec3& localOrigin, const glm::vec3& localDir, float maxDistance,
                          float& outDistance) const;

    void CalculateBounds(const aiScene* scene);
};

} // namespace Render
} // namespace Platform
} // namespace Engine



