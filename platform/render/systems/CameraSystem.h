#pragma once

#include "core/System.h"
#include "render/ICameraSource.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

// Updates the active camera each frame (after InputSystem, before RenderSystem).
class CameraSystem : public ECS::ISystem {
public:
    CameraSystem();

    void SetSource(ICameraSource* source) { m_source = source; }
    ICameraSource* GetSource() const { return m_source; }

    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;

private:
    ICameraSource* m_source = nullptr;
};

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine
