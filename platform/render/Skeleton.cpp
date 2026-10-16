#include "render/Skeleton.h"
#include "Logger.h"

#include <algorithm>
#include <functional>

namespace Engine {
namespace Platform {
namespace Render {

glm::mat4 Skeleton::AiToGlm(const aiMatrix4x4& m) {
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4);
}

Skeleton::Skeleton() = default;

Skeleton::~Skeleton() = default;

bool Skeleton::LoadSkeleton(const aiScene* scene) {
    if (!scene || !scene->mRootNode) {
        LOG_ERROR("Skeleton", "Cannot load skeleton: invalid scene or root node");
        return false;
    }
    
    m_bones.clear();
    m_boneArray.clear();
    m_boneParentMap.clear();
    m_nodeParentMap.clear();
    m_nodeBindLocal.clear();
    m_nodeChildren.clear();
    m_boneCounter = 0;
    m_maxBoneCount = 0;
    m_loaded = false;
    m_bindPoseActive = true;
    m_hierarchyRootName.clear();
    
    ExtractBonesFromMeshes(scene);
    
    if (m_bones.empty()) {
        BuildBoneHierarchy(scene->mRootNode, scene, glm::mat4(1.0f));
    }
    
    if (m_bones.empty()) {
        LOG_ERROR("Skeleton", "Failed to extract any bones from scene");
        return false;
    }
    
    m_rootBoneName = std::string(scene->mRootNode->mName.C_Str());
    m_hierarchyRootName = m_rootBoneName;
    m_globalInverseTransform = glm::inverse(AiToGlm(scene->mRootNode->mTransformation));
    
    StoreNodeHierarchy(scene->mRootNode, "");
    
    m_boneParentMap.clear();
    for (const auto& bonePair : m_bones) {
        const std::string& boneName = bonePair.first;
        std::string boneAncestor = FindBoneAncestor(boneName);
        if (!boneAncestor.empty() && boneAncestor != boneName) {
            m_boneParentMap[boneName] = boneAncestor;
        }
    }
    
    m_boneArray.assign(m_maxBoneCount, nullptr);
    for (auto& pair : m_bones) {
        Bone& bone = pair.second;
        if (bone.id >= 0 && bone.id < static_cast<int>(m_boneArray.size())) {
            m_boneArray[bone.id] = &bone;
        }
    }
    
    m_loaded = true;
    ResetToBindPose();
    LOG_INFO_FMT("Skeleton", "Skeleton loaded: %zu bones, %zu hierarchy nodes",
                 m_bones.size(), m_nodeBindLocal.size());
    
    return true;
}

void Skeleton::ExtractBonesFromMeshes(const aiScene* scene) {
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        
        if (!mesh->HasBones()) {
            continue;
        }
        
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
            const aiBone* aiBone = mesh->mBones[boneIndex];
            std::string boneName = std::string(aiBone->mName.C_Str());
            
            const aiMatrix4x4& offset = aiBone->mOffsetMatrix;
            glm::mat4 offsetMatrix = AiToGlm(offset);
            
            int boneID = GetOrCreateBoneID(boneName);
            
            if (m_bones.find(boneName) == m_bones.end()) {
                m_bones[boneName] = Bone(boneName, boneID, offsetMatrix);
            }
        }
    }
    
    m_maxBoneCount = m_bones.size();
}

void Skeleton::BuildBoneHierarchy(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform) {
    std::string nodeName = std::string(node->mName.C_Str());
    
    glm::mat4 localTransform = AiToGlm(node->mTransformation);
    glm::mat4 globalTransform = parentTransform * localTransform;
    
    auto boneIt = m_bones.find(nodeName);
    if (boneIt == m_bones.end()) {
        int boneID = GetOrCreateBoneID(nodeName);
        m_bones[nodeName] = Bone(nodeName, boneID, glm::mat4(1.0f));
        m_maxBoneCount = std::max(m_maxBoneCount, m_bones.size());
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        BuildBoneHierarchy(node->mChildren[i], scene, globalTransform);
    }
}

void Skeleton::StoreNodeHierarchy(aiNode* node, const std::string& parentName) {
    std::string nodeName = std::string(node->mName.C_Str());
    
    m_nodeBindLocal[nodeName] = AiToGlm(node->mTransformation);
    
    if (!parentName.empty()) {
        m_nodeParentMap[nodeName] = parentName;
        m_nodeChildren[parentName].push_back(nodeName);
    }
    
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        StoreNodeHierarchy(node->mChildren[i], nodeName);
    }
}

std::string Skeleton::FindBoneAncestor(const std::string& boneName) {
    std::string currentNode = boneName;
    int maxDepth = 64;
    int depth = 0;
    
    while (depth < maxDepth) {
        auto parentIt = m_nodeParentMap.find(currentNode);
        if (parentIt == m_nodeParentMap.end()) {
            break;
        }
        
        const std::string& parentName = parentIt->second;
        
        if (m_bones.find(parentName) != m_bones.end()) {
            return parentName;
        }
        
        currentNode = parentName;
        depth++;
    }
    
    return "";
}

int Skeleton::GetOrCreateBoneID(const std::string& boneName) {
    auto it = m_bones.find(boneName);
    if (it != m_bones.end()) {
        return it->second.id;
    }
    
    return m_boneCounter++;
}

Bone* Skeleton::GetBone(const std::string& boneName) {
    auto it = m_bones.find(boneName);
    if (it != m_bones.end()) {
        return &it->second;
    }
    return nullptr;
}

const Bone* Skeleton::GetBone(const std::string& boneName) const {
    auto it = m_bones.find(boneName);
    if (it != m_bones.end()) {
        return &it->second;
    }
    return nullptr;
}

Bone* Skeleton::GetBone(int boneID) {
    if (boneID >= 0 && boneID < static_cast<int>(m_boneArray.size())) {
        return m_boneArray[boneID];
    }
    return nullptr;
}

const Bone* Skeleton::GetBone(int boneID) const {
    if (boneID >= 0 && boneID < static_cast<int>(m_boneArray.size())) {
        return m_boneArray[boneID];
    }
    return nullptr;
}

void Skeleton::ResetToBindPose() {
    if (!m_loaded) {
        return;
    }
    m_bindPoseActive = true;
    for (auto& pair : m_bones) {
        pair.second.finalTransformation = glm::mat4(1.0f);
    }
}

bool Skeleton::AlignBoneIdsToMeshOrder(const std::vector<std::string>& meshBoneNamesInIndexOrder) {
    if (!m_loaded || meshBoneNamesInIndexOrder.empty()) {
        return false;
    }

    size_t matched = 0;
    int maxId = -1;
    for (size_t i = 0; i < meshBoneNamesInIndexOrder.size(); ++i) {
        const std::string& name = meshBoneNamesInIndexOrder[i];
        auto it = m_bones.find(name);
        if (it == m_bones.end()) {
            continue;
        }
        it->second.id = static_cast<int>(i);
        maxId = std::max(maxId, static_cast<int>(i));
        ++matched;
    }

    if (matched == 0) {
        return false;
    }

    m_maxBoneCount = static_cast<size_t>(maxId + 1);
    m_boneArray.assign(m_maxBoneCount, nullptr);
    for (auto& pair : m_bones) {
        Bone& bone = pair.second;
        if (bone.id >= 0 && bone.id < static_cast<int>(m_boneArray.size())) {
            m_boneArray[bone.id] = &bone;
        }
    }

    LOG_INFO_FMT("Skeleton",
                 "Aligned %zu/%zu mesh bone indices to skeleton IDs (array size %zu)",
                 matched,
                 meshBoneNamesInIndexOrder.size(),
                 m_maxBoneCount);
    return matched == meshBoneNamesInIndexOrder.size();
}

void Skeleton::ApplyNodeHierarchy(const std::string& nodeName,
                                  const glm::mat4& parentGlobal,
                                  const std::map<std::string, glm::mat4>& animationTransforms) {
    glm::mat4 localTransform(1.0f);
    auto bindIt = m_nodeBindLocal.find(nodeName);
    if (bindIt != m_nodeBindLocal.end()) {
        localTransform = bindIt->second;
    }

    auto animIt = animationTransforms.find(nodeName);
    if (animIt != animationTransforms.end()) {
        localTransform = animIt->second;
    }

    const glm::mat4 globalTransform = parentGlobal * localTransform;

    auto boneIt = m_bones.find(nodeName);
    if (boneIt != m_bones.end()) {
        boneIt->second.finalTransformation = globalTransform;
    }

    auto childrenIt = m_nodeChildren.find(nodeName);
    if (childrenIt != m_nodeChildren.end()) {
        for (const std::string& childName : childrenIt->second) {
            ApplyNodeHierarchy(childName, globalTransform, animationTransforms);
        }
    }
}

void Skeleton::UpdateBoneTransforms(const std::map<std::string, glm::mat4>& animationTransforms) {
    if (!m_loaded) {
        return;
    }

    if (animationTransforms.empty() || m_hierarchyRootName.empty()) {
        ResetToBindPose();
        return;
    }

    m_bindPoseActive = false;
    for (auto& pair : m_bones) {
        pair.second.finalTransformation = glm::mat4(1.0f);
    }

    ApplyNodeHierarchy(m_hierarchyRootName, glm::mat4(1.0f), animationTransforms);
}

void Skeleton::UpdateBoneTransformsLayered(
    const std::map<std::string, glm::mat4>& primaryTransforms,
    const std::map<std::string, glm::mat4>& overlayTransforms,
    const std::function<bool(const std::string& nodeName)>& useOverlayForNode) {
    if (!m_loaded || primaryTransforms.empty() || m_hierarchyRootName.empty()) {
        ResetToBindPose();
        return;
    }

    std::map<std::string, glm::mat4> merged = primaryTransforms;
    if (overlayTransforms.empty() || !useOverlayForNode) {
        UpdateBoneTransforms(merged);
        return;
    }

    for (const auto& entry : overlayTransforms) {
        if (useOverlayForNode(entry.first)) {
            merged[entry.first] = entry.second;
        }
    }
    UpdateBoneTransforms(merged);
}

void Skeleton::GetFinalBoneMatrices(std::vector<glm::mat4>& boneMatrices) const {
    if (!m_loaded) {
        return;
    }
    
    boneMatrices.assign(m_maxBoneCount, glm::mat4(1.0f));

    // Bind pose: caller should set boneCount=0; identity mats are a safe fallback.
    if (m_bindPoseActive) {
        return;
    }
    
    for (const auto& pair : m_bones) {
        const Bone& bone = pair.second;
        if (bone.id < 0 || bone.id >= static_cast<int>(boneMatrices.size())) {
            continue;
        }
        switch (m_matrixFormulaMode) {
            case 0:
                boneMatrices[bone.id] = m_globalInverseTransform * bone.finalTransformation * bone.offsetMatrix;
                break;
            case 1:
                boneMatrices[bone.id] = bone.finalTransformation * bone.offsetMatrix;
                break;
            case 2:
                boneMatrices[bone.id] = bone.offsetMatrix * bone.finalTransformation;
                break;
            case 3:
                boneMatrices[bone.id] = bone.offsetMatrix * bone.finalTransformation * m_globalInverseTransform;
                break;
            default:
                boneMatrices[bone.id] = m_globalInverseTransform * bone.finalTransformation * bone.offsetMatrix;
                break;
        }
    }
}

Skeleton Skeleton::Clone() const {
    Skeleton copy;
    copy.m_loaded = m_loaded;
    copy.m_bindPoseActive = true;
    copy.m_bones = m_bones;
    copy.m_maxBoneCount = m_maxBoneCount;
    copy.m_rootBoneName = m_rootBoneName;
    copy.m_hierarchyRootName = m_hierarchyRootName;
    copy.m_boneCounter = m_boneCounter;
    copy.m_globalInverseTransform = m_globalInverseTransform;
    copy.m_matrixFormulaMode = m_matrixFormulaMode;
    copy.m_boneParentMap = m_boneParentMap;
    copy.m_nodeParentMap = m_nodeParentMap;
    copy.m_nodeBindLocal = m_nodeBindLocal;
    copy.m_nodeChildren = m_nodeChildren;

    copy.m_boneArray.assign(copy.m_maxBoneCount, nullptr);
    for (auto& pair : copy.m_bones) {
        Bone& bone = pair.second;
        if (bone.id >= 0 && bone.id < static_cast<int>(copy.m_boneArray.size())) {
            copy.m_boneArray[bone.id] = &bone;
        }
    }
    copy.ResetToBindPose();
    return copy;
}

} // namespace Render
} // namespace Platform
} // namespace Engine
