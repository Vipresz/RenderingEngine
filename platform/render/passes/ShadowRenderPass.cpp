#include "render/passes/ShadowRenderPass.h"

#include "Logger.h"
#include "render/passes/RenderPassMeshDraw.h"
#include "render/components/AnimationComponent.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/TransformComponent.h"
#include "render/Model.h"
#include "render/Skeleton.h"
#include "render/SunLight.h"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int kMaxShaderBones = 100;

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

} // namespace

namespace Engine {
namespace Platform {
namespace Render {
namespace Passes {

ShadowRenderPass::ShadowRenderPass() = default;

ShadowRenderPass::~ShadowRenderPass() {
    if (m_shadowFbo != 0) {
        glDeleteFramebuffers(1, &m_shadowFbo);
        m_shadowFbo = 0;
    }
    if (m_shadowDepthTexture != 0) {
        glDeleteTextures(1, &m_shadowDepthTexture);
        m_shadowDepthTexture = 0;
    }
    m_depthShader.ClearShader();
}

void ShadowRenderPass::EnsureResources() {
    if (m_initialized) {
        return;
    }

    m_depthShader.SetWarnOnMissingUniforms(false);
    m_depthShader.CreateFromFiles("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
    if (!m_depthShader.IsValid()) {
        LOG_ERROR("ShadowRenderPass",
                  "shadow_depth shader failed to load — directional shadows disabled (check shaders/shadow_depth.*)");
        m_shaderLoadFailed = true;
        return;
    }

    glGenTextures(1, &m_shadowDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kShadowMapSize, kShadowMapSize, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &m_shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowDepthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_initialized = m_depthShader.IsValid() && m_shadowFbo != 0 && m_shadowDepthTexture != 0;
}

glm::mat4 ShadowRenderPass::ComputeLightSpaceMatrix(const RenderContext& context) const {
    const glm::vec3 lightDir = glm::normalize(SunLight::Travel());

    glm::vec3 focusCenter(0.0f, 5.0f, 0.0f);
    float focusDistance = 50.0f;
    if (context.cameraSource) {
        const glm::vec3 eye = context.cameraSource->GetEyePosition();
        glm::vec3 forward(0.0f, 0.0f, -1.0f);
        glm::vec3 right(1.0f, 0.0f, 0.0f);
        if (context.cameraSource->GetMovementBasis(forward, right)) {
            focusCenter = eye + forward * focusDistance;
        } else {
            focusCenter = eye + glm::vec3(0.0f, 0.0f, -focusDistance);
        }
    }

    float halfExtent = 55.0f;
    glm::vec3 sceneMin;
    glm::vec3 sceneMax;
    if (TryComputeVisibleSceneBounds(context, sceneMin, sceneMax)) {
        focusCenter = (sceneMin + sceneMax) * 0.5f;
        const glm::vec3 sceneHalf = (sceneMax - sceneMin) * 0.5f;
        halfExtent = std::max({sceneHalf.x, sceneHalf.y, sceneHalf.z, 5.0f});
        halfExtent = std::min(halfExtent + 4.0f, 80.0f);
    }

    const float zPadding = 25.0f;
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(lightDir, up)) > 0.95f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    const glm::vec3 lightPos = focusCenter - lightDir * 80.0f;
    const glm::mat4 lightView = glm::lookAt(lightPos, focusCenter, up);

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    const glm::vec3 corners[8] = {
        focusCenter + glm::vec3(-halfExtent, -halfExtent, -halfExtent),
        focusCenter + glm::vec3(halfExtent, -halfExtent, -halfExtent),
        focusCenter + glm::vec3(-halfExtent, halfExtent, -halfExtent),
        focusCenter + glm::vec3(halfExtent, halfExtent, -halfExtent),
        focusCenter + glm::vec3(-halfExtent, -halfExtent, halfExtent),
        focusCenter + glm::vec3(halfExtent, -halfExtent, halfExtent),
        focusCenter + glm::vec3(-halfExtent, halfExtent, halfExtent),
        focusCenter + glm::vec3(halfExtent, halfExtent, halfExtent),
    };

    for (const glm::vec3& corner : corners) {
        const glm::vec4 lightSpace = lightView * glm::vec4(corner, 1.0f);
        minX = std::min(minX, lightSpace.x);
        maxX = std::max(maxX, lightSpace.x);
        minY = std::min(minY, lightSpace.y);
        maxY = std::max(maxY, lightSpace.y);
        minZ = std::min(minZ, lightSpace.z);
        maxZ = std::max(maxZ, lightSpace.z);
    }

    const float zNear = -maxZ - zPadding;
    const float zFar = -minZ + zPadding;
    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, zNear, zFar);
    return lightProjection * lightView;
}

bool ShadowRenderPass::TryComputeVisibleSceneBounds(const RenderContext& context,
                                                   glm::vec3& outMin,
                                                   glm::vec3& outMax) const {
    if (!context.registry) {
        return false;
    }

    std::vector<MeshDrawItem> items;
    CollectMeshDrawItems(*context.registry, context, false, items);
    if (items.empty()) {
        return false;
    }

    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());
    bool found = false;

    for (const MeshDrawItem& item : items) {
        const auto* transformComp =
            context.registry->GetComponent<Engine::Platform::Render::Components::TransformComponent>(
                item.entityID);
        if (!transformComp || item.model == nullptr) {
            continue;
        }

        float minX = 0.0f;
        float maxX = 0.0f;
        float minY = 0.0f;
        float maxY = 0.0f;
        float minZ = 0.0f;
        float maxZ = 0.0f;
        item.model->GetBounds(minX, maxX, minY, maxY, minZ, maxZ);
        const glm::mat4 modelMatrix = transformComp->GetMatrix();
        const glm::vec3 corners[8] = {
            {minX, minY, minZ}, {maxX, minY, minZ}, {minX, maxY, minZ}, {maxX, maxY, minZ},
            {minX, minY, maxZ}, {maxX, minY, maxZ}, {minX, maxY, maxZ}, {maxX, maxY, maxZ},
        };
        for (const glm::vec3& corner : corners) {
            const glm::vec3 worldCorner = glm::vec3(modelMatrix * glm::vec4(corner, 1.0f));
            outMin = glm::min(outMin, worldCorner);
            outMax = glm::max(outMax, worldCorner);
            found = true;
        }
    }

    return found;
}

void ShadowRenderPass::RenderShadowCasters(RenderContext& context, const glm::mat4& lightSpaceMatrix) {
    if (!context.registry || !m_depthShader.IsValid()) {
        return;
    }

    m_depthShader.UseShader();
    const GLuint shaderProgramID = m_depthShader.GetShaderID();
    const GLint uniformModel = glGetUniformLocation(shaderProgramID, "model");
    const GLint uniformLightSpace = glGetUniformLocation(shaderProgramID, "lightSpaceMatrix");
    if (uniformLightSpace != -1) {
        glUniformMatrix4fv(uniformLightSpace, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    }

    using Components::AnimationComponent;
    using Components::TransformComponent;
    std::vector<MeshDrawItem> casters;
    CollectMeshDrawItems(*context.registry, context, false, casters);
    for (const MeshDrawItem& item : casters) {
        const TransformComponent* transformComp =
            context.registry->GetComponent<TransformComponent>(item.entityID);
        if (!transformComp || item.model == nullptr) {
            continue;
        }

        const glm::mat4 modelMatrix = transformComp->GetMatrix();
        if (uniformModel != -1) {
            glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        }

        const AnimationComponent* animComp =
            context.registry->GetComponent<AnimationComponent>(item.entityID);
        UploadBoneUniforms(shaderProgramID, animComp ? animComp->skeleton : nullptr);
        item.model->RenderModel();
    }
}

void ShadowRenderPass::Execute(RenderContext& context) {
    EnsureResources();
    if (!AreShadowsEnabled() || !context.shader) {
        if (m_shaderLoadFailed) {
            static bool loggedOnce = false;
            if (!loggedOnce) {
                loggedOnce = true;
                LOG_WARNING("ShadowRenderPass",
                            "Directional shadows disabled — shadow_depth shaders failed to load");
            }
        }
        return;
    }

    const glm::mat4 lightSpaceMatrix = ComputeLightSpaceMatrix(context);
    context.directionalLightTransform = lightSpaceMatrix;
    context.shadowMapTexture = m_shadowDepthTexture;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);
    RenderShadowCasters(context, lightSpaceMatrix);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    context.shader->UseShader();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);

    const GLuint shaderProgramID = context.shader->GetShaderID();
    const GLint uniformShadowMap = glGetUniformLocation(shaderProgramID, "directionalShadowMap");
    if (uniformShadowMap != -1) {
        glUniform1i(uniformShadowMap, 0);
    }

    context.shader->SetDirectionalLightTransform(&context.directionalLightTransform);
}

} // namespace Passes
} // namespace Render
} // namespace Platform
} // namespace Engine
