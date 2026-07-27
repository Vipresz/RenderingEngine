#pragma once

#include "core/scene/SceneNode.h"

#include <unordered_map>
#include <vector>

namespace Engine {
namespace Scene {

class Scene {
public:
    SceneNodeId CreateNode(const std::string& name, SceneNodeId parentId = kInvalidSceneNode);
    void DestroyNode(SceneNodeId nodeId);

    SceneNode* GetNode(SceneNodeId nodeId);
    const SceneNode* GetNode(SceneNodeId nodeId) const;

    SceneNodeId GetRoot() const { return m_rootId; }
    const std::unordered_map<SceneNodeId, SceneNode>& Nodes() const { return m_nodes; }

    std::vector<SceneNodeId> CollectDescendants(SceneNodeId nodeId) const;
    glm::mat4 ComputeWorldMatrix(SceneNodeId nodeId) const;

private:
    SceneNodeId m_nextId = 1;
    SceneNodeId m_rootId = kInvalidSceneNode;
    std::unordered_map<SceneNodeId, SceneNode> m_nodes;
};

} // namespace Scene
} // namespace Engine
