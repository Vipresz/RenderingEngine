#include "render/passes/RenderPassMeshDraw.h"

#include "core/ecs/Registry.h"
#include "render/components/AnimationComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/TransformComponent.h"
#include "render/Model.h"
#include "render/Skeleton.h"

#include <GL/glew.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <vector>

namespace {

constexpr int kMaxShaderBones = 100;
constexpr float kOpaqueCutoff = 0.999f;

void UploadBoneUniforms(GLuint shaderProgramID, const Engine::Platform::Render::Skeleton* skeleton) {
    const GLint uniformBoneCount = glGetUniformLocation(shaderProgramID, "boneCount");
    const GLint uniformBoneTransforms = glGetUniformLocation(shaderProgramID, "boneTransforms");

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

bool PassesOpacityFilter(const Engine::Platform::Render::Components::MaterialComponent* materialComp,
                         bool transparentPass) {
    const float opacity = materialComp ? materialComp->opacity : 1.0f;
    return transparentPass ? opacity < kOpaqueCutoff : opacity >= kOpaqueCutoff;
}

} // namespace

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

void CollectTransparentDrawItems(ECS::Registry& registry,
                                 const glm::vec3& eyePosition,
                                 std::vector<TransparentDrawItem>& outItems) {
    outItems.clear();
    auto entities = registry.View<Components::MeshComponent, Components::TransformComponent>();
    for (EntityID entityID : entities) {
        const auto* meshComp = registry.GetComponent<Components::MeshComponent>(entityID);
        const auto* transformComp = registry.GetComponent<Components::TransformComponent>(entityID);
        const auto* materialComp = registry.GetComponent<Components::MaterialComponent>(entityID);
        if (!meshComp || !transformComp || !meshComp->IsLoaded()) {
            continue;
        }
        if (!PassesOpacityFilter(materialComp, true)) {
            continue;
        }

        const glm::vec3 worldPos = glm::vec3(transformComp->GetMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        TransparentDrawItem item;
        item.entityID = entityID;
        const glm::vec3 delta = worldPos - eyePosition;
        item.distanceSq = glm::dot(delta, delta);
        outItems.push_back(item);
    }

    std::sort(outItems.begin(), outItems.end(), [](const TransparentDrawItem& a, const TransparentDrawItem& b) {
        return a.distanceSq > b.distanceSq;
    });
}

void DrawMeshEntities(RenderContext& context,
                      const std::vector<EntityID>& entities,
                      bool transparentPass) {
    if (!context.registry || !context.shader || !context.cameraSource) {
        return;
    }

    context.shader->UseShader();
    if (!context.shader->IsValid()) {
        return;
    }

    const GLuint shaderProgramID = context.shader->GetShaderID();
    const GLint uniformModel = context.shader->GetModelLocation();
    const GLint uniformUseTexture = context.shader->GetUseTextureLocation();
    const GLint uniformSpecularIntensity = context.shader->GetSpecularIntensityLocation();
    const GLint uniformShininess = context.shader->GetShininessLocation();
    const GLint uniformMaterialOpacity = glGetUniformLocation(shaderProgramID, "materialOpacity");

    if (transparentPass) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }

    for (EntityID entityID : entities) {
        auto* meshComp = context.registry->GetComponent<Components::MeshComponent>(entityID);
        auto* transformComp = context.registry->GetComponent<Components::TransformComponent>(entityID);
        auto* materialComp = context.registry->GetComponent<Components::MaterialComponent>(entityID);
        if (!meshComp || !transformComp || !meshComp->IsLoaded()) {
            continue;
        }
        if (!PassesOpacityFilter(materialComp, transparentPass)) {
            continue;
        }

        const glm::mat4 modelMatrix = transformComp->GetMatrix();
        if (uniformModel != -1) {
            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        }

        GLfloat specularIntensity = 0.5f;
        GLfloat shininess = 32.0f;
        GLfloat opacity = 1.0f;
        if (materialComp) {
            specularIntensity = materialComp->specularIntensity;
            shininess = materialComp->shininess;
            opacity = materialComp->opacity;
        }

        if (uniformSpecularIntensity != -1) {
            glUniform1f(uniformSpecularIntensity, specularIntensity);
        }
        if (uniformShininess != -1) {
            glUniform1f(uniformShininess, shininess);
        }
        if (uniformMaterialOpacity != -1) {
            glUniform1f(uniformMaterialOpacity, opacity);
        }

        const auto* animComp = context.registry->GetComponent<Components::AnimationComponent>(entityID);
        UploadBoneUniforms(shaderProgramID, animComp ? animComp->skeleton : nullptr);

        if (meshComp->model) {
            meshComp->model->RenderModelPerMeshTexture(uniformUseTexture);
        }
    }

    if (transparentPass) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
