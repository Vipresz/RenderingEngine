#pragma once

#include "render/RenderPass.h"

#include <memory>
#include <vector>

namespace Engine {
namespace Platform {
namespace Render {

class RenderPipeline {
public:
    void AddPass(std::unique_ptr<RenderPass> pass);
    void ClearPasses();

    void Execute(RenderContext& context);

private:
    std::vector<std::unique_ptr<RenderPass>> m_passes;
};

} // namespace Render
} // namespace Platform
} // namespace Engine
