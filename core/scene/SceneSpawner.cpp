#include "core/scene/SceneSpawner.h"

#include "core/scene/SceneSerializer.h"
#include "render/components/TransformComponent.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace Engine {
namespace Scene {

namespace {

using Engine::Platform::Render::Components::TransformComponent;

glm::vec3 ExtractTranslation(const glm::mat4& matrix) {
    return glm::vec3(matrix[3]);
}

glm::vec3 ExtractRotationDegrees(const glm::mat4& matrix) {
    const glm::vec3 column0(matrix[0]);
    const glm::vec3 column2(matrix[2]);
    const float yawRadians = std::atan2(column0.z, column0.x);
    const float pitchRadians = std::asin(-column0.y);
    const float rollRadians = std::atan2(column2.y, column2.z);
    constexpr float kRadToDeg = 57.2957795f;
    return glm::vec3(pitchRadians * kRadToDeg, yawRadians * kRadToDeg, rollRadians * kRadToDeg);
}

glm::vec3 ExtractScale(const glm::mat4& matrix) {
    return glm::vec3(glm::length(glm::vec3(matrix[0])),
                     glm::length(glm::vec3(matrix[1])),
                     glm::length(glm::vec3(matrix[2])));
}

void ApplyWorldMatrixToTransform(TransformComponent& transform, const glm::mat4& worldMatrix) {
    transform.position = ExtractTranslation(worldMatrix);
    transform.rotation = ExtractRotationDegrees(worldMatrix);
    transform.scale = ExtractScale(worldMatrix);
}

std::string ReadTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

} // namespace

SpawnedPrefab SceneSpawner::InstantiatePrefab(ECS::Registry& registry,
                                              const Prefab& prefab,
                                              const glm::vec3& worldOrigin) {
    SpawnedPrefab spawned;
    if (prefab.nodes.empty()) {
        return spawned;
    }

    spawned.entities.reserve(prefab.nodes.size());
    for (size_t index = 0; index < prefab.nodes.size(); ++index) {
        const SceneNode& node = prefab.nodes[index];
        const EntityID entity = registry.CreateEntity().id;
        auto& transform = registry.AddComponent<TransformComponent>(entity);
        transform.position = worldOrigin + node.localPosition;
        transform.rotation = node.localRotationDegrees;
        transform.scale = node.localScale;
        spawned.entities.push_back(entity);
        spawned.nodeIndexToEntity[index] = entity;
    }

    return spawned;
}

SpawnedScene SceneSpawner::InstantiateScene(ECS::Registry& registry, Scene& scene) {
    SpawnedScene spawned;
    for (const auto& entry : scene.Nodes()) {
        const SceneNode& node = entry.second;
        if (!node.active) {
            continue;
        }

        EntityID entity = node.entity;
        if (entity == INVALID_ENTITY) {
            entity = registry.CreateEntity().id;
            scene.GetNode(entry.first)->entity = entity;
        }

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform) {
            transform = &registry.AddComponent<TransformComponent>(entity);
        }

        const glm::mat4 worldMatrix = scene.ComputeWorldMatrix(entry.first);
        ApplyWorldMatrixToTransform(*transform, worldMatrix);
        spawned.nodeToEntity[entry.first] = entity;
    }

    return spawned;
}

bool SceneSpawner::LoadAndInstantiateScene(ECS::Registry& registry, const std::string& jsonPath) {
    const std::string json = ReadTextFile(jsonPath);
    if (json.empty()) {
        return false;
    }

    Scene scene;
    if (!SceneSerializer::DeserializeScene(json, scene)) {
        return false;
    }

    InstantiateScene(registry, scene);
    return true;
}

} // namespace Scene
} // namespace Engine
