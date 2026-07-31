#include "Logger.h"
#include "render/systems/RenderSystem.h"
#include "render/Camera.h"
#include "render/FollowCamera.h"
#include "render/ICameraSource.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/Model.h"
#include "render/Skeleton.h"
#include "render/passes/OpaqueRenderPass.h"
#include "render/passes/ShadowRenderPass.h"
#include "render/passes/TransparentRenderPass.h"
#include <algorithm>
#include <GL/glew.h>
#include <cstring>
#include <memory>

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

static void CheckGLError(const char* location) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char* errorStr = nullptr;
        switch (error) {
            case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorStr = "Unknown GL error"; break;
        }
        LOG_ERROR_FMT("RenderSystem", "OpenGL Error at %s: %s (0x%X)", location, errorStr, error);
    }
}

RenderSystem::RenderSystem(Shader* shader, ICameraSource* cameraSource, const glm::mat4& projection)
    : m_shader(shader), m_cameraSource(cameraSource), m_projection(projection) {
    SetPriority(100);
}

RenderSystem::RenderSystem(Shader* shader, Camera* camera, const glm::mat4& projection)
    : RenderSystem(shader, static_cast<ICameraSource*>(camera), projection) {}

RenderSystem::RenderSystem(Shader* shader, FollowCamera* followCamera, const glm::mat4& projection)
    : RenderSystem(shader, static_cast<ICameraSource*>(followCamera), projection) {}

void RenderSystem::SetCamera(Camera* camera) {
    m_cameraSource = camera;
}

void RenderSystem::SetFollowCamera(FollowCamera* followCamera) {
    m_cameraSource = followCamera;
}

void RenderSystem::Initialize(ECS::Registry& registry) {
    LOG_INFO("RenderSystem", "Initializing render system...");
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    CreateDefaultShadowMap();
    CreateDefaultWhiteTexture();

    m_pipeline.ClearPasses();
    m_pipeline.AddPass(std::make_unique<Passes::ShadowRenderPass>());
    m_pipeline.AddPass(std::make_unique<Passes::OpaqueRenderPass>());
    m_pipeline.AddPass(std::make_unique<Passes::TransparentRenderPass>());

    LOG_INFO("RenderSystem", "Render system initialized");
}

// ============================================================================
// RENDERING - COMPLETELY INDEPENDENT OF MOVEMENT
// This system only reads camera and entity positions - it never modifies them
// All movement is handled in InputSystem
// ============================================================================
void RenderSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    if (!m_shader || !m_cameraSource) {
        return;
    }
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderContext context;
    context.registry = &registry;
    context.shader = m_shader;
    context.cameraSource = m_cameraSource;
    context.projection = m_projection;
    context.view = m_cameraSource->GetViewMatrix();
    context.viewProjection = context.projection * context.view;
    context.frustumCullingEnabled = true;
    context.deltaTime = deltaTime;
    context.tickNumber = tickNumber;
    context.defaultWhiteTexture = m_defaultWhiteTexture;
    context.shadowMapTexture = m_defaultShadowMapTexture;
    m_pipeline.Execute(context);
}

void RenderSystem::Shutdown(ECS::Registry& registry) {
    if (m_defaultShadowMapTexture != 0) {
        glDeleteTextures(1, &m_defaultShadowMapTexture);
        m_defaultShadowMapTexture = 0;
    }
    if (m_defaultWhiteTexture != 0) {
        glDeleteTextures(1, &m_defaultWhiteTexture);
        m_defaultWhiteTexture = 0;
    }
}

void RenderSystem::CreateDefaultShadowMap() {
    glGenTextures(1, &m_defaultShadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, m_defaultShadowMapTexture);
    
    unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderSystem::BindDefaultShadowMap() {
    if (m_defaultShadowMapTexture == 0) return;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_defaultShadowMapTexture);
    
    GLuint shaderProgramID = m_shader->GetShaderID();
    GLint uniformShadowMap = glGetUniformLocation(shaderProgramID, "directionalShadowMap");
    if (uniformShadowMap != -1) {
        glUniform1i(uniformShadowMap, 0);
    }
}

void RenderSystem::CreateDefaultWhiteTexture() {
    glGenTextures(1, &m_defaultWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
    
    unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderSystem::BindDefaultWhiteTexture() {
    if (m_defaultWhiteTexture == 0) return;
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_defaultWhiteTexture);
}

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine
