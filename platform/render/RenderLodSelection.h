#pragma once

#include "render/components/MeshComponent.h"
#include "render/components/MeshLodComponent.h"
#include "render/components/TransformComponent.h"

#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Render {

inline float ComputeViewDistanceSq(const Components::TransformComponent& transform,
                                   const glm::vec3& eyePosition) {
    const glm::vec3 worldPos = glm::vec3(transform.GetMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    const glm::vec3 delta = worldPos - eyePosition;
    return glm::dot(delta, delta);
}

inline Model* SelectLodModel(const Components::MeshComponent& mesh,
                             const Components::MeshLodComponent* lodComponent,
                             float viewDistance) {
    if (lodComponent == nullptr || lodComponent->levels.empty()) {
        return mesh.model;
    }

    for (const Components::MeshLodLevel& level : lodComponent->levels) {
        if (level.model != nullptr && viewDistance <= level.maxViewDistance) {
            return level.model;
        }
    }

    const Components::MeshLodLevel& fallback = lodComponent->levels.back();
    return fallback.model != nullptr ? fallback.model : mesh.model;
}

} // namespace Render
} // namespace Platform
} // namespace Engine
