#pragma once

#include "render/RenderPass.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

class TransparentRenderPass : public RenderPass {
public:
    RenderPassType GetType() const override { return RenderPassType::Transparent; }
    const char* GetName() const override { return "Transparent"; }
    void Execute(RenderContext& context) override;
};

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
