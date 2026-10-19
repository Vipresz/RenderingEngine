#pragma once

#include "core/System.h"
#include "core/Types.h"
#include "core/ecs/Registry.h"
#include "core/ecs/Entity.h"
#include "core/network/InputBuffer.h"
#include "render/Window.h"
#include "render/components/TransformComponent.h"
#include <functional>
#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Render {
    class Camera;
    class FollowCamera;
    class ICameraSource;
    class Model;
}
namespace Input {
namespace Systems {

// Input system - handles keyboard and mouse input
class InputSystem : public ECS::ISystem {
public:
    InputSystem(Window* window);
    
    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;
    
    // Pluggable camera (preferred)
    void SetCameraSource(Render::ICameraSource* cameraSource) { m_cameraSource = cameraSource; }
    Render::ICameraSource* GetCameraSource() const { return m_cameraSource; }

    // Legacy — still supported
    void SetCamera(Render::Camera* camera);
    void SetFollowCamera(Render::FollowCamera* followCamera);
    
    // Character entity tracking
    void SetCharacterEntity(EntityID entityID) { m_characterEntityID = entityID; }
    EntityID GetCharacterEntity() const { return m_characterEntityID; }
    
    // Character movement speeds (world units / second)
    void SetMovementSpeed(float walkSpeed) {
        m_walkSpeed = walkSpeed;
        if (m_runSpeed < m_walkSpeed) {
            m_runSpeed = m_walkSpeed;
        }
    }
    void SetMovementSpeeds(float walkSpeed, float runSpeed) {
        m_walkSpeed = walkSpeed;
        m_runSpeed = runSpeed > walkSpeed ? runSpeed : walkSpeed;
    }
    float GetMovementSpeed() const { return m_walkSpeed; }
    float GetWalkSpeed() const { return m_walkSpeed; }
    float GetRunSpeed() const { return m_runSpeed; }
    
    // Terrain for height sampling (character adheres to terrain)
    void SetTerrainModel(Render::Model* terrain) { m_terrainModel = terrain; }

    // UI input routing — return true to block game/camera input
    void SetInputBlockQuery(std::function<bool()> query) { m_inputBlockQuery = std::move(query); }
    void SetEscapeCallback(std::function<void()> callback) { m_escapeCallback = std::move(callback); }

    // Optional auto-walk (e.g. follow another player). Manual movement cancels via callback.
    using FollowAutoMoveQuery = std::function<bool(glm::vec3& outDirection, bool& outRun)>;
    void SetFollowAutoMoveQuery(FollowAutoMoveQuery query) {
        m_followAutoMoveQuery = std::move(query);
    }
    void SetFollowCancelCallback(std::function<void()> callback) {
        m_followCancelCallback = std::move(callback);
    }

    // When false, game code handles camera mouse/scroll (e.g. LMB-only orbit)
    void SetBuiltInCameraInputEnabled(bool enabled) { m_builtInCameraInput = enabled; }

    Engine::Net::InputBuffer& GetInputBuffer() { return m_inputBuffer; }
    const Engine::Net::InputBuffer& GetInputBuffer() const { return m_inputBuffer; }

private:
    Window* m_window = nullptr;
    Render::ICameraSource* m_cameraSource = nullptr;
    Render::Model* m_terrainModel = nullptr;
    EntityID m_characterEntityID = 0; // Track which entity is the character
    float m_walkSpeed = 2.8f;
    float m_runSpeed = 5.25f;
    
    // Input state
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_spacePressedLastFrame = false; // Track previous frame space key state for jump
    bool m_escapePressedLastFrame = false;
    std::function<bool()> m_inputBlockQuery;
    std::function<void()> m_escapeCallback;
    FollowAutoMoveQuery m_followAutoMoveQuery;
    std::function<void()> m_followCancelCallback;
    bool m_builtInCameraInput = true;
    Engine::Net::InputBuffer m_inputBuffer;
    
    bool IsGameInputBlocked() const { return m_inputBlockQuery && m_inputBlockQuery(); }
    void HandleCameraInput(float deltaTime);
    void HandleCharacterMovement(float deltaTime, ECS::Registry& registry, uint64_t tickNumber);
};

} // namespace Systems
} // namespace Input
} // namespace Platform
} // namespace Engine

