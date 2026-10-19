#pragma once

#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <map>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {

// Bone structure for skeleton
struct Bone {
    std::string name;
    int id;
    glm::mat4 offsetMatrix; // Inverse bind pose matrix (bone space to model space)
    glm::mat4 finalTransformation = glm::mat4(1.0f); // Global bone transform (animated)
    
    Bone() : id(-1), offsetMatrix(1.0f) {}
    Bone(const std::string& boneName, int boneID, const glm::mat4& offset)
        : name(boneName), id(boneID), offsetMatrix(offset) {}
};

// Skeleton class - manages bone hierarchy and transforms
class Skeleton {
public:
    Skeleton();
    ~Skeleton();
    
    // Load skeleton from Assimp scene (from FBX file)
    bool LoadSkeleton(const aiScene* scene);
    
    Bone* GetBone(const std::string& boneName);
    const Bone* GetBone(const std::string& boneName) const;
    
    Bone* GetBone(int boneID);
    const Bone* GetBone(int boneID) const;
    
    const std::map<std::string, Bone>& GetBones() const { return m_bones; }
    
    size_t GetBoneCount() const { return m_bones.size(); }
    
    /// Node-local pose. Missing channels keep the FBX bind-pose local matrix.
    /// An empty map, or a skeleton with no hierarchy root, resets to bind pose.
    void UpdateBoneTransforms(const std::map<std::string, glm::mat4>& animationTransforms);

    /// Writes \p primaryTransforms, then replaces channels for which
    /// \p useOverlayForNode returns true. \p useOverlayForNode may be null.
    void UpdateBoneTransformsLayered(
        const std::map<std::string, glm::mat4>& primaryTransforms,
        const std::map<std::string, glm::mat4>& overlayTransforms,
        bool (*useOverlayForNode)(const std::string& nodeName));

    // Skip GPU skinning (boneCount=0). Mesh verts stay in authored bind pose.
    void ResetToBindPose();
    bool IsBindPoseActive() const { return m_bindPoseActive; }
    void SetBindPoseActive(bool active) { m_bindPoseActive = active; }
    
    void GetFinalBoneMatrices(std::vector<glm::mat4>& boneMatrices) const;
    
    size_t GetMaxBoneCount() const { return m_maxBoneCount; }
    
    bool IsLoaded() const { return m_loaded; }
    
    std::string GetRootBoneName() const { return m_rootBoneName; }

    // Force skeleton bone IDs to match mesh VBO bone indices (by name order from Model).
    // Required when mesh and skeleton are loaded via separate Assimp imports.
    bool AlignBoneIdsToMeshOrder(const std::vector<std::string>& meshBoneNamesInIndexOrder);
    
    // Deep copy of hierarchy/bind data for per-character pose state (clips stay shared).
    Skeleton Clone() const;
    
    /// Skinning matrix per bone id.
    /// 0: inverse(root) * global * offset (Assimp). 1: global * offset.
    /// 2: offset * global. 3: offset * global * inverse(root).
    void SetMatrixFormulaMode(int mode) { m_matrixFormulaMode = mode; }
    int GetMatrixFormulaMode() const { return m_matrixFormulaMode; }

private:
    bool m_loaded = false;
    bool m_bindPoseActive = true;
    std::map<std::string, Bone> m_bones;
    std::vector<Bone*> m_boneArray;
    size_t m_maxBoneCount = 0;
    std::string m_rootBoneName;
    std::string m_hierarchyRootName;
    int m_boneCounter = 0;
    glm::mat4 m_globalInverseTransform{1.0f};
    int m_matrixFormulaMode = 0;

    std::map<std::string, std::string> m_nodeParentMap;
    std::map<std::string, glm::mat4> m_nodeBindLocal;
    std::map<std::string, std::vector<std::string>> m_nodeChildren;
    
    void ExtractBonesFromMeshes(const aiScene* scene);
    void BuildBoneHierarchy(aiNode* node);
    void StoreNodeHierarchy(aiNode* node, const std::string& parentName);
    int GetOrCreateBoneID(const std::string& boneName);
    void ApplyNodeHierarchy(const std::string& nodeName,
                            const glm::mat4& parentGlobal,
                            const std::map<std::string, glm::mat4>& animationTransforms);

    static glm::mat4 AiToGlm(const aiMatrix4x4& m);
};

} // namespace Render
} // namespace Platform
} // namespace Engine
