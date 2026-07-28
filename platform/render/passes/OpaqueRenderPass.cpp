#include "render/passes/OpaqueRenderPass.h"

#include "Logger.h"
#include "render/passes/RenderPassMeshDraw.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/TransformComponent.h"

#include <GL/glew.h>

#include <vector>

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

void OpaqueRenderPass::Execute(RenderContext& context) {
    if (!context.registry || !context.shader || !context.cameraSource) {
        return;
    }

    context.shader->UseShader();
    if (!context.shader->IsValid()) {
        return;
    }

    const glm::mat4 view = context.cameraSource->GetViewMatrix();
    const glm::vec3 eyePos = context.cameraSource->GetEyePosition();

    const GLint uniformView = context.shader->GetViewLocation();
    const GLint uniformProjection = context.shader->GetProjectionLocation();
    const GLint uniformEyePosition = context.shader->GetEyePositionLocation();
    const GLuint shaderProgramID = context.shader->GetShaderID();

    if (uniformView == -1 || uniformProjection == -1) {
        static bool loggedMissingCameraUniforms = false;
        if (!loggedMissingCameraUniforms) {
            loggedMissingCameraUniforms = true;
            LOG_ERROR("OpaqueRenderPass",
                      "Lit shader missing view/projection uniforms — opaque meshes will not render");
        }
        return;
    }

    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(context.projection));
    if (uniformEyePosition != -1) {
        glUniform3f(uniformEyePosition, eyePos.x, eyePos.y, eyePos.z);
    }

    context.shader->SetDirectionalLightTransform(&context.directionalLightTransform);

    if (context.shadowMapTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, context.shadowMapTexture);
        const GLint uniformShadowMap = glGetUniformLocation(shaderProgramID, "directionalShadowMap");
        if (uniformShadowMap != -1) {
            glUniform1i(uniformShadowMap, 0);
        }
    }

    const GLint uniformPointLightCount = glGetUniformLocation(shaderProgramID, "pointLightCount");
    const GLint uniformSpotLightCount = glGetUniformLocation(shaderProgramID, "spotLightCount");
    const GLint uniformDirectionalLightColour = glGetUniformLocation(shaderProgramID, "directionalLight.base.colour");
    const GLint uniformDirectionalLightAmbient = glGetUniformLocation(shaderProgramID, "directionalLight.base.ambientIntensity");
    const GLint uniformDirectionalLightDiffuse = glGetUniformLocation(shaderProgramID, "directionalLight.base.diffuseIntensity");
    const GLint uniformDirectionalLightDirection = glGetUniformLocation(shaderProgramID, "directionalLight.direction");
    const GLint uniformMaterialOpacity = glGetUniformLocation(shaderProgramID, "materialOpacity");

    if (uniformPointLightCount != -1) glUniform1i(uniformPointLightCount, 0);
    if (uniformSpotLightCount != -1) glUniform1i(uniformSpotLightCount, 0);
    if (uniformDirectionalLightColour != -1) glUniform3f(uniformDirectionalLightColour, 1.0f, 1.0f, 1.0f);
    if (uniformDirectionalLightAmbient != -1) glUniform1f(uniformDirectionalLightAmbient, 0.3f);
    if (uniformDirectionalLightDiffuse != -1) glUniform1f(uniformDirectionalLightDiffuse, 0.7f);
    if (uniformDirectionalLightDirection != -1) glUniform3f(uniformDirectionalLightDirection, 0.3f, -1.0f, 0.2f);
    if (uniformMaterialOpacity != -1) glUniform1f(uniformMaterialOpacity, 1.0f);

    context.shader->SetTexture(1);
    if (context.defaultWhiteTexture != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, context.defaultWhiteTexture);
    }

    std::vector<EntityID> opaqueEntities;
    auto entities = context.registry->View<Components::MeshComponent, Components::TransformComponent>();
    for (EntityID entityID : entities) {
        const auto* materialComp = context.registry->GetComponent<Components::MaterialComponent>(entityID);
        const float opacity = materialComp ? materialComp->opacity : 1.0f;
        if (opacity >= 0.999f) {
            opaqueEntities.push_back(entityID);
        }
    }

    DrawMeshEntities(context, opaqueEntities, false);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
