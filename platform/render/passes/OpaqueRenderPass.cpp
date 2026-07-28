#include "render/passes/OpaqueRenderPass.h"

#include "Logger.h"
#include "render/components/AnimationComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/TransformComponent.h"
#include "render/Model.h"
#include "render/Skeleton.h"

#include <GL/glew.h>

#include <algorithm>
#include <vector>

namespace {

constexpr int kMaxShaderBones = 100;

void UploadBoneUniforms(GLuint shaderProgramID, const Engine::Platform::Render::Skeleton* skeleton) {
    const GLint uniformBoneCount = glGetUniformLocation(shaderProgramID, "boneCount");
    const GLint uniformBoneTransforms = glGetUniformLocation(shaderProgramID, "boneTransforms");

    // Bind pose / no skeleton: disable GPU skinning so mesh verts stay as authored.
    int boneCount = 0;
    if (skeleton && skeleton->IsLoaded() && !skeleton->IsBindPoseActive()) {
        std::vector<glm::mat4> boneMatrices;
        skeleton->GetFinalBoneMatrices(boneMatrices);
        boneCount = static_cast<int>(std::min(boneMatrices.size(), static_cast<size_t>(kMaxShaderBones)));
        if (uniformBoneTransforms != -1 && boneCount > 0) {
            glUniformMatrix4fv(uniformBoneTransforms, boneCount, GL_FALSE, glm::value_ptr(boneMatrices[0]));
        }
    }

    if (uniformBoneCount != -1) {
        glUniform1i(uniformBoneCount, boneCount);
    }
}

} // namespace

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

    if (uniformPointLightCount != -1) glUniform1i(uniformPointLightCount, 0);
    if (uniformSpotLightCount != -1) glUniform1i(uniformSpotLightCount, 0);
    if (uniformDirectionalLightColour != -1) glUniform3f(uniformDirectionalLightColour, 1.0f, 1.0f, 1.0f);
    if (uniformDirectionalLightAmbient != -1) glUniform1f(uniformDirectionalLightAmbient, 0.3f);
    if (uniformDirectionalLightDiffuse != -1) glUniform1f(uniformDirectionalLightDiffuse, 0.7f);
    if (uniformDirectionalLightDirection != -1) glUniform3f(uniformDirectionalLightDirection, 0.3f, -1.0f, 0.2f);

    context.shader->SetTexture(1);
    if (context.defaultWhiteTexture != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, context.defaultWhiteTexture);
    }

    const GLint uniformUseTexture = context.shader->GetUseTextureLocation();

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

        const auto* animComp = context.registry->GetComponent<Components::AnimationComponent>(entityID);
        UploadBoneUniforms(shaderProgramID, animComp ? animComp->skeleton : nullptr);

        if (meshComp->model) {
            meshComp->model->RenderModelPerMeshTexture(uniformUseTexture);
        }
    }
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
