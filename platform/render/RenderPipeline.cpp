#include "render/RenderPipeline.h"

namespace Engine {
namespace Platform {
namespace Render {

void RenderPipeline::AddPass(std::unique_ptr<RenderPass> pass) {
    if (pass) {
        m_passes.push_back(std::move(pass));
    }
}

void RenderPipeline::ClearPasses() {
    m_passes.clear();
}

void RenderPipeline::Execute(RenderContext& context) {
    for (const std::unique_ptr<RenderPass>& pass : m_passes) {
        if (pass) {
            pass->Execute(context);
        }
    }
}

} // namespace Render
} // namespace Platform
} // namespace Engine
