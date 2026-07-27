#include "render/systems/CameraSystem.h"
#include "Logger.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

CameraSystem::CameraSystem() {
    SetPriority(10); // After InputSystem (0), before AnimationSystem (50) and RenderSystem (100)
}

void CameraSystem::Initialize(ECS::Registry& /*registry*/) {
    LOG_INFO("CameraSystem", "CameraSystem initialized");
}

void CameraSystem::Update(float deltaTime, ECS::Registry& /*registry*/, uint64_t /*tickNumber*/) {
    if (m_source) {
        m_source->Update(deltaTime);
    }
}

void CameraSystem::Shutdown(ECS::Registry& /*registry*/) {}

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine
