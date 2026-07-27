#pragma once

#include "render/RenderPass.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

class ShadowRenderPass : public RenderPass {
public:
    RenderPassType GetType() const override { return RenderPassType::Shadow; }
    const char* GetName() const override { return "Shadow"; }
    void Execute(RenderContext& context) override;
};

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
