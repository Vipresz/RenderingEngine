#pragma once

#include "render/RenderPass.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

class OpaqueRenderPass : public RenderPass {
public:
    RenderPassType GetType() const override { return RenderPassType::Opaque; }
    const char* GetName() const override { return "Opaque"; }
    void Execute(RenderContext& context) override;
};

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
