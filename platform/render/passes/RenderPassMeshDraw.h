#pragma once

#include "render/RenderPass.h"

#include <vector>

namespace Engine {
namespace ECS {
class Registry;
}
namespace Platform {
namespace Render {

class Model;

struct MeshDrawItem {
    EntityID entityID = 0;
    Model* model = nullptr;
    float distanceSq = 0.0f;
};

namespace Passes {

void CollectMeshDrawItems(ECS::Registry& registry,
                          const RenderContext& context,
                          bool transparentPass,
                          std::vector<MeshDrawItem>& outItems);

void DrawMeshDrawItems(RenderContext& context,
                       const std::vector<MeshDrawItem>& items,
                       bool transparentPass);

struct TransparentDrawItem {
    EntityID entityID = 0;
    float distanceSq = 0.0f;
};

void CollectTransparentDrawItems(ECS::Registry& registry,
                                 const glm::vec3& eyePosition,
                                 std::vector<TransparentDrawItem>& outItems);

void DrawMeshEntities(RenderContext& context,
                      const std::vector<EntityID>& entities,
                      bool transparentPass);

void SetupLitShaderCameraUniforms(RenderContext& context);

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
