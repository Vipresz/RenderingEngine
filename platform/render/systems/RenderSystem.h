#pragma once

#include "core/System.h"
#include "core/ecs/Registry.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/Shader.h"
#include "render/ICameraSource.h"
#include "render/RenderPipeline.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {
namespace Platform {
namespace Render {

class Camera;
class FollowCamera;

namespace Systems {

class RenderSystem : public ECS::ISystem {
public:
    RenderSystem(Shader* shader, ICameraSource* cameraSource, const glm::mat4& projection);

    // Convenience — existing code can still pass concrete camera types
    RenderSystem(Shader* shader, Camera* camera, const glm::mat4& projection);
    RenderSystem(Shader* shader, FollowCamera* followCamera, const glm::mat4& projection);
    
    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;
    
    void SetProjection(const glm::mat4& projection) { m_projection = projection; }
    void SetShader(Shader* shader) { m_shader = shader; }
    void SetCameraSource(ICameraSource* cameraSource) { m_cameraSource = cameraSource; }

    // Legacy setters
    void SetCamera(Camera* camera);
    void SetFollowCamera(FollowCamera* followCamera);
    
    RenderPipeline& GetPipeline() { return m_pipeline; }
    const RenderPipeline& GetPipeline() const { return m_pipeline; }
    
private:
    Shader* m_shader = nullptr;
    ICameraSource* m_cameraSource = nullptr;
    glm::mat4 m_projection;
    RenderPipeline m_pipeline;
    
    GLuint m_defaultShadowMapTexture = 0;
    GLuint m_defaultWhiteTexture = 0;
    
    void CreateDefaultShadowMap();
    void BindDefaultShadowMap();
    void CreateDefaultWhiteTexture();
    void BindDefaultWhiteTexture();
};

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine
