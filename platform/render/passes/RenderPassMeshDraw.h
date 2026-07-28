#pragma once

#include "render/RenderPass.h"

#include <vector>

namespace Engine {
namespace ECS {
class Registry;
}
namespace Platform {
namespace Render {

struct TransparentDrawItem {
    EntityID entityID = 0;
    float distanceSq = 0.0f;
};

namespace Passes {

void CollectTransparentDrawItems(ECS::Registry& registry,
                                 const glm::vec3& eyePosition,
                                 std::vector<TransparentDrawItem>& outItems);

void DrawMeshEntities(RenderContext& context,
                      const std::vector<EntityID>& entities,
                      bool transparentPass);

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
