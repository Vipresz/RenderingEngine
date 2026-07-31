#include "render/passes/RenderPassMeshDraw.h"

#include "core/ecs/Registry.h"
#include "render/Frustum.h"
#include "render/RenderLodSelection.h"
#include "render/components/AnimationComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MeshLodComponent.h"
#include "render/components/TransformComponent.h"
#include "render/Model.h"
#include "render/Skeleton.h"

#include <GL/glew.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <limits>
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

bool ComputeWorldBounds(const Engine::Platform::Render::Model& model,
                        const glm::mat4& modelMatrix,
                        glm::vec3& outMin,
                        glm::vec3& outMax) {
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    model.GetBounds(minX, maxX, minY, maxY, minZ, maxZ);

    const glm::vec3 localMin(minX, minY, minZ);
    const glm::vec3 localMax(maxX, maxY, maxZ);
    const glm::vec3 corners[8] = {
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
    };

    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (const glm::vec3& corner : corners) {
        const glm::vec3 worldCorner = glm::vec3(modelMatrix * glm::vec4(corner, 1.0f));
        outMin = glm::min(outMin, worldCorner);
        outMax = glm::max(outMax, worldCorner);
    }
    return true;
}

} // namespace

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

void SetupLitShaderCameraUniforms(RenderContext& context) {
    if (!context.shader || !context.cameraSource) {
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

    if (uniformView != -1) {
        glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    }
    if (uniformProjection != -1) {
        glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(context.projection));
    }
    if (uniformEyePosition != -1) {
        glUniform3f(uniformEyePosition, eyePos.x, eyePos.y, eyePos.z);
    }
}

void CollectMeshDrawItems(ECS::Registry& registry,
                          const RenderContext& context,
                          bool transparentPass,
                          std::vector<MeshDrawItem>& outItems) {
    outItems.clear();
    if (!context.cameraSource) {
        return;
    }

    const glm::vec3 eyePos = context.cameraSource->GetEyePosition();
    const Frustum frustum =
        context.frustumCullingEnabled ? ExtractFrustum(context.viewProjection) : Frustum{};

    auto entities = registry.View<Components::MeshComponent, Components::TransformComponent>();
    for (EntityID entityID : entities) {
        const auto* meshComp = registry.GetComponent<Components::MeshComponent>(entityID);
        const auto* transformComp = registry.GetComponent<Components::TransformComponent>(entityID);
        const auto* materialComp = registry.GetComponent<Components::MaterialComponent>(entityID);
        const auto* lodComp = registry.GetComponent<Components::MeshLodComponent>(entityID);
        if (!meshComp || !transformComp || !meshComp->IsLoaded()) {
            continue;
        }
        if (!PassesOpacityFilter(materialComp, transparentPass)) {
            continue;
        }

        const float distanceSq = ComputeViewDistanceSq(*transformComp, eyePos);
        const float viewDistance = std::sqrt(distanceSq);
        Model* drawModel = SelectLodModel(*meshComp, lodComp, viewDistance);
        if (drawModel == nullptr || drawModel->GetMeshCount() == 0) {
            continue;
        }

        const glm::mat4 modelMatrix = transformComp->GetMatrix();
        if (context.frustumCullingEnabled) {
            glm::vec3 boundsMin;
            glm::vec3 boundsMax;
            ComputeWorldBounds(*drawModel, modelMatrix, boundsMin, boundsMax);
            if (!IntersectsAABB(frustum, boundsMin, boundsMax)) {
                continue;
            }
        }

        MeshDrawItem item;
        item.entityID = entityID;
        item.model = drawModel;
        item.distanceSq = distanceSq;
        outItems.push_back(item);
    }

    if (transparentPass) {
        std::sort(outItems.begin(), outItems.end(), [](const MeshDrawItem& a, const MeshDrawItem& b) {
            return a.distanceSq > b.distanceSq;
        });
    }
}

void DrawMeshDrawItems(RenderContext& context,
                       const std::vector<MeshDrawItem>& items,
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

    for (const MeshDrawItem& item : items) {
        auto* transformComp = context.registry->GetComponent<Components::TransformComponent>(item.entityID);
        auto* materialComp = context.registry->GetComponent<Components::MaterialComponent>(item.entityID);
        if (!transformComp || item.model == nullptr) {
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

        const auto* animComp = context.registry->GetComponent<Components::AnimationComponent>(item.entityID);
        UploadBoneUniforms(shaderProgramID, animComp ? animComp->skeleton : nullptr);
        item.model->RenderModelPerMeshTexture(uniformUseTexture);
    }

    if (transparentPass) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void CollectTransparentDrawItems(ECS::Registry& registry,
                                 const glm::vec3& eyePosition,
                                 std::vector<TransparentDrawItem>& outItems) {
    outItems.clear();
    RenderContext context;
    context.cameraSource = nullptr;
    context.frustumCullingEnabled = false;

    std::vector<MeshDrawItem> items;
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
        TransparentDrawItem item;
        item.entityID = entityID;
        item.distanceSq = ComputeViewDistanceSq(*transformComp, eyePosition);
        outItems.push_back(item);
    }

    std::sort(outItems.begin(), outItems.end(), [](const TransparentDrawItem& a, const TransparentDrawItem& b) {
        return a.distanceSq > b.distanceSq;
    });
}

void DrawMeshEntities(RenderContext& context,
                      const std::vector<EntityID>& entities,
                      bool transparentPass) {
    std::vector<MeshDrawItem> items;
    items.reserve(entities.size());
    for (EntityID entityID : entities) {
        if (!context.registry) {
            continue;
        }
        const auto* meshComp = context.registry->GetComponent<Components::MeshComponent>(entityID);
        const auto* transformComp = context.registry->GetComponent<Components::TransformComponent>(entityID);
        const auto* lodComp = context.registry->GetComponent<Components::MeshLodComponent>(entityID);
        if (!meshComp || !transformComp) {
            continue;
        }
        MeshDrawItem item;
        item.entityID = entityID;
        item.distanceSq = context.cameraSource
                              ? ComputeViewDistanceSq(*transformComp, context.cameraSource->GetEyePosition())
                              : 0.0f;
        item.model = SelectLodModel(
            *meshComp,
            lodComp,
            std::sqrt(item.distanceSq));
        items.push_back(item);
    }
    DrawMeshDrawItems(context, items, transparentPass);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
