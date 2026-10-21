#include "render/passes/OpaqueRenderPass.h"

#include "Logger.h"
#include "render/SunLight.h"
#include "render/passes/RenderPassMeshDraw.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/TransformComponent.h"

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <vector>

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

void OpaqueRenderPass::Execute(RenderContext& context) {
    if (!context.registry || !context.shader || !context.cameraSource) {
        return;
    }

    SetupLitShaderCameraUniforms(context);
    context.shader->UseShader();
    if (!context.shader->IsValid()) {
        return;
    }

    const GLint uniformView = context.shader->GetViewLocation();
    const GLint uniformProjection = context.shader->GetProjectionLocation();
    if (uniformView == -1 || uniformProjection == -1) {
        static bool loggedMissingCameraUniforms = false;
        if (!loggedMissingCameraUniforms) {
            loggedMissingCameraUniforms = true;
            LOG_ERROR("OpaqueRenderPass",
                      "Lit shader missing view/projection uniforms — opaque meshes will not render");
        }
        return;
    }

    context.shader->SetDirectionalLightTransform(&context.directionalLightTransform);

    if (context.shadowMapTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, context.shadowMapTexture);
        const GLint uniformShadowMap =
            glGetUniformLocation(context.shader->GetShaderID(), "directionalShadowMap");
        if (uniformShadowMap != -1) {
            glUniform1i(uniformShadowMap, 0);
        }
    }

    const GLuint shaderProgramID = context.shader->GetShaderID();
    const GLint uniformPointLightCount = glGetUniformLocation(shaderProgramID, "pointLightCount");
    const GLint uniformSpotLightCount = glGetUniformLocation(shaderProgramID, "spotLightCount");
    const GLint uniformDirectionalLightColour =
        glGetUniformLocation(shaderProgramID, "directionalLight.base.colour");
    const GLint uniformDirectionalLightAmbient =
        glGetUniformLocation(shaderProgramID, "directionalLight.base.ambientIntensity");
    const GLint uniformDirectionalLightDiffuse =
        glGetUniformLocation(shaderProgramID, "directionalLight.base.diffuseIntensity");
    const GLint uniformDirectionalLightDirection =
        glGetUniformLocation(shaderProgramID, "directionalLight.direction");
    const GLint uniformMaterialOpacity = glGetUniformLocation(shaderProgramID, "materialOpacity");

    if (uniformPointLightCount != -1) {
        glUniform1i(uniformPointLightCount, 0);
    }
    if (uniformSpotLightCount != -1) {
        glUniform1i(uniformSpotLightCount, 0);
    }
    const glm::vec3 sunColour = SunLight::Colour();
    const glm::vec3 sunTravel = SunLight::Travel();
    if (uniformDirectionalLightColour != -1) {
        glUniform3f(uniformDirectionalLightColour, sunColour.r, sunColour.g, sunColour.b);
    }
    if (uniformDirectionalLightAmbient != -1) {
        glUniform1f(uniformDirectionalLightAmbient, SunLight::Ambient());
    }
    if (uniformDirectionalLightDiffuse != -1) {
        glUniform1f(uniformDirectionalLightDiffuse, SunLight::Diffuse());
    }
    if (uniformDirectionalLightDirection != -1) {
        glUniform3f(uniformDirectionalLightDirection, sunTravel.x, sunTravel.y, sunTravel.z);
    }
    if (uniformMaterialOpacity != -1) {
        glUniform1f(uniformMaterialOpacity, 1.0f);
    }

    context.shader->SetTexture(1);
    if (context.defaultWhiteTexture != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, context.defaultWhiteTexture);
    }

    std::vector<MeshDrawItem> opaqueItems;
    CollectMeshDrawItems(*context.registry, context, false, opaqueItems);
    DrawMeshDrawItems(context, opaqueItems, false);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
