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
    bool AreShadowsEnabled() const { return m_initialized && !m_shaderLoadFailed; }

private:
    void EnsureResources();
    void RenderShadowCasters(RenderContext& context, const glm::mat4& lightSpaceMatrix);
    glm::mat4 ComputeLightSpaceMatrix(const RenderContext& context) const;
    bool TryComputeVisibleSceneBounds(const RenderContext& context,
                                      glm::vec3& outMin,
                                      glm::vec3& outMax) const;

    Shader m_depthShader;
    GLuint m_shadowFbo = 0;
    GLuint m_shadowDepthTexture = 0;
    bool m_initialized = false;
    bool m_shaderLoadFailed = false;
    static constexpr int kShadowMapSize = 2048;
};

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
