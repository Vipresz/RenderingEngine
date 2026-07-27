#pragma once

#include "core/ecs/Registry.h"
#include "render/ICameraSource.h"
#include "render/Shader.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {
namespace Platform {
namespace Render {

struct RenderContext {
    ECS::Registry* registry = nullptr;
    Shader* shader = nullptr;
    ICameraSource* cameraSource = nullptr;
    glm::mat4 projection{1.0f};
    float deltaTime = 0.0f;
    uint64_t tickNumber = 0;
    GLuint defaultWhiteTexture = 0;
    GLuint shadowMapTexture = 0;
};

enum class RenderPassType {
    Shadow,
    Opaque,
    Transparent,
    UI,
};

class RenderPass {
public:
    virtual ~RenderPass() = default;

    virtual RenderPassType GetType() const = 0;
    virtual const char* GetName() const = 0;
    virtual void Execute(RenderContext& context) = 0;
};

} // namespace Render
} // namespace Platform
} // namespace Engine
