#include "core/scene/Scene.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Engine {
namespace Scene {

namespace {

glm::mat4 LocalMatrix(const SceneNode& node) {
    glm::mat4 matrix = glm::mat4(1.0f);
    matrix = glm::translate(matrix, node.localPosition);
    matrix = glm::rotate(matrix, glm::radians(node.localRotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(node.localRotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(node.localRotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    matrix = glm::scale(matrix, node.localScale);
    return matrix;
}

} // namespace

SceneNodeId Scene::CreateNode(const std::string& name, SceneNodeId parentId) {
    const SceneNodeId id = m_nextId++;
    SceneNode node;
    node.id = id;
    node.parentId = parentId;
    node.name = name;
    m_nodes[id] = node;

    if (parentId == kInvalidSceneNode) {
        if (m_rootId == kInvalidSceneNode) {
            m_rootId = id;
        }
    } else if (SceneNode* parent = GetNode(parentId)) {
        parent->children.push_back(id);
    }

    return id;
}

void Scene::DestroyNode(SceneNodeId nodeId) {
    const auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return;
    }

    std::vector<SceneNodeId> descendants = CollectDescendants(nodeId);
    for (SceneNodeId childId : descendants) {
        m_nodes.erase(childId);
    }
    m_nodes.erase(it);

    if (m_rootId == nodeId) {
        m_rootId = kInvalidSceneNode;
    }
}

void Scene::Clear() {
    m_nodes.clear();
    m_rootId = kInvalidSceneNode;
    m_nextId = 1;
}

SceneNode* Scene::GetNode(SceneNodeId nodeId) {
    auto it = m_nodes.find(nodeId);
    return it == m_nodes.end() ? nullptr : &it->second;
}

const SceneNode* Scene::GetNode(SceneNodeId nodeId) const {
    auto it = m_nodes.find(nodeId);
    return it == m_nodes.end() ? nullptr : &it->second;
}

std::vector<SceneNodeId> Scene::CollectDescendants(SceneNodeId nodeId) const {
    std::vector<SceneNodeId> result;
    const SceneNode* node = GetNode(nodeId);
    if (!node) {
        return result;
    }

    for (SceneNodeId childId : node->children) {
        result.push_back(childId);
        const std::vector<SceneNodeId> nested = CollectDescendants(childId);
        result.insert(result.end(), nested.begin(), nested.end());
    }
    return result;
}

glm::mat4 Scene::ComputeWorldMatrix(SceneNodeId nodeId) const {
    const SceneNode* node = GetNode(nodeId);
    if (!node) {
        return glm::mat4(1.0f);
    }

    glm::mat4 world = LocalMatrix(*node);
    if (node->parentId != kInvalidSceneNode) {
        world = ComputeWorldMatrix(node->parentId) * world;
    }
    return world;
}

} // namespace Scene
} // namespace Engine
