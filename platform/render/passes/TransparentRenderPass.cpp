#include "render/passes/TransparentRenderPass.h"

#include <GL/glew.h>

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

void TransparentRenderPass::Execute(RenderContext& context) {
    (void)context;
    // Reserved for alpha-blended geometry sorted back-to-front.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
