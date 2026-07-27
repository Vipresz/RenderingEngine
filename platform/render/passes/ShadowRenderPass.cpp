#include "render/passes/ShadowRenderPass.h"

#include <GL/glew.h>

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

void ShadowRenderPass::Execute(RenderContext& context) {
    if (!context.shader || context.shadowMapTexture == 0) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, context.shadowMapTexture);

    const GLuint shaderProgramID = context.shader->GetShaderID();
    const GLint uniformShadowMap = glGetUniformLocation(shaderProgramID, "directionalShadowMap");
    if (uniformShadowMap != -1) {
        glUniform1i(uniformShadowMap, 0);
    }

    glm::mat4 lightTransform = glm::mat4(1.0f);
    context.shader->SetDirectionalLightTransform(&lightTransform);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
