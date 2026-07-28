#include "render/passes/TransparentRenderPass.h"

#include "render/passes/RenderPassMeshDraw.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

void TransparentRenderPass::Execute(RenderContext& context) {
    if (!context.registry || !context.shader || !context.cameraSource) {
        return;
    }

    const glm::vec3 eyePos = context.cameraSource->GetEyePosition();
    std::vector<TransparentDrawItem> transparentItems;
    CollectTransparentDrawItems(*context.registry, eyePos, transparentItems);
    if (transparentItems.empty()) {
        return;
    }

    std::vector<EntityID> entityOrder;
    entityOrder.reserve(transparentItems.size());
    for (const TransparentDrawItem& item : transparentItems) {
        entityOrder.push_back(item.entityID);
    }

    DrawMeshEntities(context, entityOrder, true);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
