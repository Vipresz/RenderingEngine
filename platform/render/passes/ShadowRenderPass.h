#pragma once

#include "render/RenderPass.h"
#include "render/Shader.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

class ShadowRenderPass : public RenderPass {
public:
    ShadowRenderPass();
    ~ShadowRenderPass() override;

    RenderPassType GetType() const override { return RenderPassType::Shadow; }
    const char* GetName() const override { return "Shadow"; }
    void Execute(RenderContext& context) override;

private:
    void EnsureResources();
    void RenderShadowCasters(RenderContext& context, const glm::mat4& lightSpaceMatrix);
    glm::mat4 ComputeLightSpaceMatrix(const RenderContext& context) const;

    Shader m_depthShader;
    GLuint m_shadowFbo = 0;
    GLuint m_shadowDepthTexture = 0;
    bool m_initialized = false;
    static constexpr int kShadowMapSize = 2048;
    glm::vec3 m_lightDirection{0.3f, -1.0f, 0.2f};
};

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
