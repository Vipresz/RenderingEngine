#include "render/passes/OpaqueRenderPass.h"

#include "render/components/MaterialComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/TransformComponent.h"
#include "render/Model.h"

#include <GL/glew.h>

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

    const GLint uniformModel = context.shader->GetModelLocation();
    const GLint uniformView = context.shader->GetViewLocation();
    const GLint uniformProjection = context.shader->GetProjectionLocation();
    const GLint uniformEyePosition = context.shader->GetEyePositionLocation();

    if (uniformView == -1 || uniformProjection == -1) {
        return;
    }

    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(context.projection));
    if (uniformEyePosition != -1) {
        glUniform3f(uniformEyePosition, eyePos.x, eyePos.y, eyePos.z);
    }

    const GLuint shaderProgramID = context.shader->GetShaderID();
    const GLint uniformPointLightCount = glGetUniformLocation(shaderProgramID, "pointLightCount");
    const GLint uniformSpotLightCount = glGetUniformLocation(shaderProgramID, "spotLightCount");
    const GLint uniformDirectionalLightColour = glGetUniformLocation(shaderProgramID, "directionalLight.base.colour");
    const GLint uniformDirectionalLightAmbient = glGetUniformLocation(shaderProgramID, "directionalLight.base.ambientIntensity");
    const GLint uniformDirectionalLightDiffuse = glGetUniformLocation(shaderProgramID, "directionalLight.base.diffuseIntensity");
    const GLint uniformDirectionalLightDirection = glGetUniformLocation(shaderProgramID, "directionalLight.direction");

    if (uniformPointLightCount != -1) glUniform1i(uniformPointLightCount, 0);
    if (uniformSpotLightCount != -1) glUniform1i(uniformSpotLightCount, 0);
    if (uniformDirectionalLightColour != -1) glUniform3f(uniformDirectionalLightColour, 1.0f, 1.0f, 1.0f);
    if (uniformDirectionalLightAmbient != -1) glUniform1f(uniformDirectionalLightAmbient, 0.3f);
    if (uniformDirectionalLightDiffuse != -1) glUniform1f(uniformDirectionalLightDiffuse, 0.7f);
    if (uniformDirectionalLightDirection != -1) glUniform3f(uniformDirectionalLightDirection, 0.0f, -1.0f, -0.5f);

    context.shader->SetTexture(1);
    if (context.defaultWhiteTexture != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, context.defaultWhiteTexture);
    }

    auto entities = context.registry->View<Components::MeshComponent, Components::TransformComponent>();
    for (EntityID entityID : entities) {
        auto* meshComp = context.registry->GetComponent<Components::MeshComponent>(entityID);
        auto* transformComp = context.registry->GetComponent<Components::TransformComponent>(entityID);
        auto* materialComp = context.registry->GetComponent<Components::MaterialComponent>(entityID);
        if (!meshComp || !transformComp || !meshComp->IsLoaded()) {
            continue;
        }

        const glm::mat4 modelMatrix = transformComp->GetMatrix();
        if (uniformModel != -1) {
            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        }

        GLfloat specularIntensity = 0.5f;
        GLfloat shininess = 32.0f;
        if (materialComp) {
            specularIntensity = materialComp->specularIntensity;
            shininess = materialComp->shininess;
        }

        const GLint uniformSpecularIntensity = context.shader->GetSpecularIntensityLocation();
        const GLint uniformShininess = context.shader->GetShininessLocation();
        if (uniformSpecularIntensity != -1) glUniform1f(uniformSpecularIntensity, specularIntensity);
        if (uniformShininess != -1) glUniform1f(uniformShininess, shininess);

        const GLint uniformBoneCount = glGetUniformLocation(shaderProgramID, "boneCount");
        if (uniformBoneCount != -1) {
            glUniform1i(uniformBoneCount, 0);
        }

        if (meshComp->model) {
            meshComp->model->RenderModel();
        }
    }
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
