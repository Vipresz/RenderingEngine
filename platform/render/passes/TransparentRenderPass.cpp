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

    SetupLitShaderCameraUniforms(context);

    std::vector<MeshDrawItem> transparentItems;
    CollectMeshDrawItems(*context.registry, context, true, transparentItems);
    if (transparentItems.empty()) {
        return;
    }

    DrawMeshDrawItems(context, transparentItems, true);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
