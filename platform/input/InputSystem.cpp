#include "input/InputSystem.h"
#include "core/network/CharacterMovementSimulator.h"
#include "core/network/MovementCommand.h"
#include "render/Camera.h"
#include "render/FollowCamera.h"
#include "render/ICameraSource.h"
#include "render/Model.h"
#include "render/components/TransformComponent.h"
#include "render/components/AnimationComponent.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cmath>

namespace Engine {
namespace Platform {
namespace Input {
namespace Systems {

namespace {

using Engine::Net::CharacterMovementSimulator;
using Engine::Net::CharacterMovementState;
using Engine::Net::MovementAnimationState;
using Engine::Net::MovementCommand;

MovementAnimationState ToMovementAnimationState(
    Render::Components::AnimationComponent::AnimationState state) {
    switch (state) {
        case Render::Components::AnimationComponent::AnimationState::Walking:
            return MovementAnimationState::Walking;
        case Render::Components::AnimationComponent::AnimationState::Running:
            return MovementAnimationState::Running;
        case Render::Components::AnimationComponent::AnimationState::Jumping:
            return MovementAnimationState::Jumping;
        case Render::Components::AnimationComponent::AnimationState::TurningLeft:
            return MovementAnimationState::TurningLeft;
        case Render::Components::AnimationComponent::AnimationState::TurningRight:
            return MovementAnimationState::TurningRight;
        case Render::Components::AnimationComponent::AnimationState::Idle:
        default:
            return MovementAnimationState::Idle;
    }
}

Render::Components::AnimationComponent::AnimationState FromMovementAnimationState(
    MovementAnimationState state) {
    switch (state) {
        case MovementAnimationState::Walking:
            return Render::Components::AnimationComponent::AnimationState::Walking;
        case MovementAnimationState::Running:
            return Render::Components::AnimationComponent::AnimationState::Running;
        case MovementAnimationState::Jumping:
            return Render::Components::AnimationComponent::AnimationState::Jumping;
        case MovementAnimationState::TurningLeft:
            return Render::Components::AnimationComponent::AnimationState::TurningLeft;
        case MovementAnimationState::TurningRight:
            return Render::Components::AnimationComponent::AnimationState::TurningRight;
        case MovementAnimationState::Idle:
        default:
            return Render::Components::AnimationComponent::AnimationState::Idle;
    }
}

CharacterMovementState BuildMovementState(
    const Render::Components::TransformComponent& transform,
    const Render::Components::AnimationComponent* animComp) {
    CharacterMovementState state;
    state.position = transform.position;
    state.yawDegrees = transform.GetYaw();
    if (animComp) {
        state.isJumping = animComp->isJumping;
        state.jumpVelocity = animComp->jumpVelocity;
        state.groundLevel = animComp->groundLevel;
        state.gravity = animComp->gravity;
        state.jumpInitialVelocity = animComp->jumpInitialVelocity;
        state.airVelocityX = animComp->airVelocityX;
        state.airVelocityZ = animComp->airVelocityZ;
        state.jumpAnimationActive = animComp->jumpAnimationActive;
        state.animationState = ToMovementAnimationState(animComp->currentState);
    }
    return state;
}

void ApplyMovementState(Render::Components::TransformComponent& transform,
                        Render::Components::AnimationComponent* animComp,
                        const CharacterMovementState& state) {
    transform.position = state.position;
    transform.SetYaw(state.yawDegrees);
    if (!animComp) {
        return;
    }

    animComp->isJumping = state.isJumping;
    animComp->jumpVelocity = state.jumpVelocity;
    animComp->groundLevel = state.groundLevel;
    animComp->airVelocityX = state.airVelocityX;
    animComp->airVelocityZ = state.airVelocityZ;
    animComp->jumpAnimationActive = state.jumpAnimationActive;
    animComp->currentState = FromMovementAnimationState(state.animationState);
    animComp->isRunning = state.animationState == MovementAnimationState::Running;
    animComp->locomotionSpeed =
        std::sqrt(state.airVelocityX * state.airVelocityX + state.airVelocityZ * state.airVelocityZ);
}

} // namespace

InputSystem::InputSystem(Window* window) : m_window(window) {
    SetPriority(0);
}

void InputSystem::SetCamera(Render::Camera* camera) {
    m_cameraSource = camera;
}

void InputSystem::SetFollowCamera(Render::FollowCamera* followCamera) {
    m_cameraSource = followCamera;
}

void InputSystem::Initialize(ECS::Registry& registry) {
    (void)registry;
}

void InputSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    if (!m_window) {
        return;
    }

    m_window->pollEvents();

    bool* keys = m_window->getsKeys();
    bool escapeJustPressed = keys[GLFW_KEY_ESCAPE] && !m_escapePressedLastFrame;
    m_escapePressedLastFrame = keys[GLFW_KEY_ESCAPE];
    if (escapeJustPressed && m_escapeCallback) {
        m_escapeCallback();
    }

    if (!IsGameInputBlocked()) {
        HandleCharacterMovement(deltaTime, registry, tickNumber);
        if (m_builtInCameraInput) {
            HandleCameraInput(deltaTime);
        }
    }
}

void InputSystem::Shutdown(ECS::Registry& registry) {
    (void)registry;
}

void InputSystem::HandleCameraInput(float deltaTime) {
    (void)deltaTime;
    if (!m_window || IsGameInputBlocked() || !m_cameraSource) {
        return;
    }

    GLfloat xChange = m_window->getXChange();
    GLfloat yChange = m_window->getYChange();
    if (xChange != 0.0f || yChange != 0.0f) {
        m_cameraSource->OnMouseMove(xChange, yChange);
    }

    GLfloat scrollChange = m_window->getScrollYChange();
    if (scrollChange != 0.0f) {
        m_cameraSource->OnScroll(scrollChange);
    }
}

void InputSystem::HandleCharacterMovement(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    if (!m_window || m_characterEntityID == 0) {
        return;
    }

    auto* transform = registry.GetComponent<Render::Components::TransformComponent>(m_characterEntityID);
    if (!transform) {
        return;
    }

    auto* animComp = registry.GetComponent<Render::Components::AnimationComponent>(m_characterEntityID);
    bool* keys = m_window->getsKeys();

    glm::vec3 moveDirection(0.0f);
    glm::vec3 forwardDir(0.0f, 0.0f, 0.0f);
    glm::vec3 rightDir(0.0f, 0.0f, 0.0f);

    if (m_cameraSource && m_cameraSource->GetMovementBasis(forwardDir, rightDir)) {
    } else {
        forwardDir = glm::vec3(0.0f, 0.0f, -1.0f);
        rightDir = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP]) {
        moveDirection += forwardDir;
    }
    if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN]) {
        moveDirection -= forwardDir;
    }
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT]) {
        moveDirection -= rightDir;
    }
    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) {
        moveDirection += rightDir;
    }

    const bool isRunning =
        (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) &&
        (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP]);
    const bool isMoving = glm::length(moveDirection) > 0.0f;
    if (isMoving) {
        moveDirection = glm::normalize(moveDirection);
    }

    const bool rmbHeld = m_window->isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT) ||
        (m_window->GetGLFWWindow() &&
         glfwGetMouseButton(m_window->GetGLFWWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

    bool spaceJustPressed = false;
    if (animComp) {
        const bool isOnGround = std::abs(transform->position.y - animComp->groundLevel) < 0.2f;
        spaceJustPressed = keys[GLFW_KEY_SPACE] && !m_spacePressedLastFrame && isOnGround;
        m_spacePressedLastFrame = keys[GLFW_KEY_SPACE];
    }

    MovementCommand command;
    command.tick = tickNumber;
    command.deltaTime = deltaTime;
    command.moveX = moveDirection.x;
    command.moveZ = moveDirection.z;
    command.run = isRunning;
    command.jumpPressed = spaceJustPressed;
    command.faceMovement = !rmbHeld;
    command.facingYawDegrees = transform->GetYaw();
    if (rmbHeld && m_cameraSource) {
        command.facingYawDegrees = m_cameraSource->GetPlanarFacingYawDegrees();
    }
    // movementSpeed is the final horizontal speed (server sanitize uses the same stats).
    command.movementSpeed = isRunning ? m_runSpeed : m_walkSpeed;
    m_inputBuffer.Push(command);

    CharacterMovementState state = BuildMovementState(*transform, animComp);
    const auto terrainSampler = m_terrainModel
        ? Engine::Net::TerrainHeightSampler(
              [terrain = m_terrainModel](float x, float z) { return terrain->GetHeightAt(x, z); })
        : Engine::Net::TerrainHeightSampler();
    CharacterMovementSimulator::SimulateStep(state, command, terrainSampler);
    ApplyMovementState(*transform, animComp, state);
}

} // namespace Systems
} // namespace Input
} // namespace Platform
} // namespace Engine
