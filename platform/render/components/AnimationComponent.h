#pragma once

// Forward declaration
namespace Engine { namespace Platform { namespace Render { class Animation; } } }
namespace Engine { namespace Platform { namespace Render { class Skeleton; } } }

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

// Animation component - stores animation state and physics for an entity
struct AnimationComponent {
    // Current animation state
    enum class AnimationState {
        Idle = 0,
        Walking,
        Running,
        Jumping,
        TurningLeft,
        TurningRight
    };
    
    AnimationState currentState = AnimationState::Idle;
    
    // Animation references (managed externally, set by main.cpp)
    Animation* idleAnimation = nullptr;
    Animation* walkingAnimation = nullptr;
    Animation* runningAnimation = nullptr;
    Animation* jumpAnimation = nullptr; // Standing / in-place jump
    Animation* jumpMovingAnimation = nullptr; // Jump while walking/running
    Animation* fallingAnimation = nullptr; // Loop after jump clip ends while still airborne
    Animation* landingAnimation = nullptr; // One-shot on ground contact after airtime
    Animation* leftTurnAnimation = nullptr; // Standing turn-in-place (RMB look)
    Animation* rightTurnAnimation = nullptr;
    
    // Skeleton reference (managed externally, set by main.cpp)
    Skeleton* skeleton = nullptr;
    
    // Current animation playback
    Animation* currentAnimation = nullptr;
    float currentAnimationTime = 0.0f; // Current time in seconds
    bool isLooping = true;
    
    // Note: Bone transforms are stored in the Skeleton, not here (to keep component POD-compliant)
    
    // Jump physics state
    bool isJumping = false;
    float jumpVelocity = 0.0f; // Current vertical velocity (units per second)
    float gravity = -9.81f; // Gravity acceleration (units per second^2)
    float jumpInitialVelocity = 5.0f; // Initial jump velocity when space is pressed
    float groundLevel = 0.0f; // Y position of the ground (for landing detection)
    float airVelocityX = 0.0f;
    float airVelocityZ = 0.0f;
    bool jumpAnimationActive = false;
    bool wasAirborneLastFrame = false; // Edge-detect touchdown for landing clip
    float fallingTimeThisAirtime = 0.0f; // Seconds spent on Falling this airtime
    bool landingActive = false; // Playing landing one-shot
    
    // Movement state
    bool isRunning = false; // True when Shift+W is pressed
    bool wasMovingLastFrame = false; // Track movement state changes

    // Horizontal speed from the movement sim (units/sec). Drives walk/run clip rate.
    float locomotionSpeed = 0.0f;
    // Travel speed each clip was authored to represent at 1x playback.
    float walkAnimReferenceSpeed = 2.5f;
    float runAnimReferenceSpeed = 5.0f;

    // Combat skill overlay (upper body by default; full body when skillFullBody is true).
    bool skillActive = false;
    bool skillFullBody = false;
    Animation* skillAnimation = nullptr;
    float skillAnimationTime = 0.0f;
    float skillDurationSeconds = 1.0f;
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine

