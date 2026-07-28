#include "render/systems/AnimationSystem.h"
#include "render/components/AnimationComponent.h"
#include "render/Animation.h"
#include "render/Skeleton.h"
#include "Logger.h"
#include <cmath>
#include <algorithm>
#include <map>

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

namespace {

float ResolveLocomotionPlaybackRate(const Components::AnimationComponent& animComp,
                                    float cycleDurationSeconds) {
    const bool isRun =
        animComp.currentState == Components::AnimationComponent::AnimationState::Running;
    const bool isWalk =
        animComp.currentState == Components::AnimationComponent::AnimationState::Walking;
    if (!isRun && !isWalk) {
        return 1.0f;
    }

    float speed = animComp.locomotionSpeed;
    if (speed < 0.01f) {
        // Fallback if movement hasn't written speed yet this frame.
        speed = isRun ? animComp.runAnimReferenceSpeed * 0.7f : animComp.walkAnimReferenceSpeed * 0.56f;
    }

    const float referenceSpeed =
        isRun ? animComp.runAnimReferenceSpeed : animComp.walkAnimReferenceSpeed;
    if (referenceSpeed <= 0.01f) {
        return 1.0f;
    }

    // Match foot-cycle rate to travel speed. Mixamo clips are usually authored faster than
    // our gameplay walk/run speeds, so this scales playback down.
    (void)cycleDurationSeconds;
    return std::clamp(speed / referenceSpeed, 0.15f, 2.5f);
}

bool IsJumpTakeoffClip(const Components::AnimationComponent& animComp, const Animation* clip) {
    return clip != nullptr &&
           (clip == animComp.jumpAnimation || clip == animComp.jumpMovingAnimation);
}

bool ClipFinished(const Animation* clip, float animationTimeSeconds) {
    if (!clip || !clip->IsLoaded()) {
        return true;
    }
    const float tps = clip->GetTicksPerSecond();
    const float durationTicks = clip->GetDuration();
    if (tps <= 0.0f || durationTicks <= 0.0f) {
        return true;
    }
    return animationTimeSeconds >= (durationTicks / tps) - 0.0001f;
}

bool JumpClipFinished(const Components::AnimationComponent& animComp) {
    if (!IsJumpTakeoffClip(animComp, animComp.currentAnimation)) {
        return false;
    }
    return ClipFinished(animComp.currentAnimation, animComp.currentAnimationTime);
}

float ResolvePlaybackRate(const Components::AnimationComponent& animComp,
                          float clipDurationSeconds) {
    (void)clipDurationSeconds;
    // Jump clips are cosmetic; physics owns vertical/horizontal travel — never time-scale to airtime.
    if (animComp.currentState == Components::AnimationComponent::AnimationState::Jumping &&
        IsJumpTakeoffClip(animComp, animComp.currentAnimation)) {
        return 1.0f;
    }
    return ResolveLocomotionPlaybackRate(animComp, clipDurationSeconds);
}

} // namespace

AnimationSystem::AnimationSystem() {
    SetPriority(50); // Run after InputSystem (0) but before RenderSystem (100)
}

void AnimationSystem::Initialize(ECS::Registry& registry) {
    LOG_INFO("AnimationSystem", "AnimationSystem initialized");
}

void AnimationSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    (void)tickNumber;
    UpdateAnimationPlayback(deltaTime, registry);
}

void AnimationSystem::Shutdown(ECS::Registry& registry) {
    (void)registry;
}

void AnimationSystem::UpdateAnimationPlayback(float deltaTime, ECS::Registry& registry) {
    auto entities = registry.View<Components::AnimationComponent>();
    
    for (auto entityID : entities) {
        auto* animComp = registry.GetComponent<Components::AnimationComponent>(entityID);
        if (!animComp) {
            continue;
        }

        const bool airborne =
            animComp->isJumping ||
            animComp->currentState == Components::AnimationComponent::AnimationState::Jumping;
        constexpr float kMinFallingSecondsForLanding = 0.5f;

        if (airborne) {
            animComp->landingActive = false;
        }

        // Touchdown: Landing only if Falling ran longer than the threshold.
        if (animComp->wasAirborneLastFrame && !airborne) {
            if (animComp->fallingTimeThisAirtime >= kMinFallingSecondsForLanding &&
                animComp->landingAnimation) {
                animComp->landingActive = true;
                animComp->currentAnimation = animComp->landingAnimation;
                animComp->currentAnimationTime = 0.0f;
                animComp->isLooping = false;
            }
            animComp->fallingTimeThisAirtime = 0.0f;
        }
        if (animComp->landingActive &&
            ClipFinished(animComp->landingAnimation, animComp->currentAnimationTime)) {
            animComp->landingActive = false;
        }
        animComp->wasAirborneLastFrame = airborne;
        
        Animation* targetAnimation = nullptr;

        if (animComp->landingActive && animComp->landingAnimation) {
            targetAnimation = animComp->landingAnimation;
        } else {
            switch (animComp->currentState) {
                case Components::AnimationComponent::AnimationState::Idle:
                    targetAnimation = animComp->idleAnimation;
                    break;
                case Components::AnimationComponent::AnimationState::Walking:
                    targetAnimation = animComp->walkingAnimation;
                    break;
                case Components::AnimationComponent::AnimationState::Running:
                    targetAnimation = animComp->runningAnimation;
                    break;
                case Components::AnimationComponent::AnimationState::TurningLeft:
                    targetAnimation = animComp->leftTurnAnimation
                        ? animComp->leftTurnAnimation
                        : animComp->idleAnimation;
                    break;
                case Components::AnimationComponent::AnimationState::TurningRight:
                    targetAnimation = animComp->rightTurnAnimation
                        ? animComp->rightTurnAnimation
                        : animComp->idleAnimation;
                    break;
                case Components::AnimationComponent::AnimationState::Jumping: {
                    // Takeoff clip (in-place or moving) until it finishes, then Falling
                    // while still airborne. Stick to the chosen takeoff so mid-air key
                    // release doesn't swap Moving_jump <-> Jump_inplace.
                    if (animComp->currentAnimation == animComp->fallingAnimation &&
                        animComp->fallingAnimation) {
                        targetAnimation = animComp->fallingAnimation;
                    } else if (IsJumpTakeoffClip(*animComp, animComp->currentAnimation)) {
                        if (JumpClipFinished(*animComp) && animComp->fallingAnimation) {
                            targetAnimation = animComp->fallingAnimation;
                        } else {
                            targetAnimation = animComp->currentAnimation;
                        }
                    } else {
                        const float airSpeedSq =
                            animComp->airVelocityX * animComp->airVelocityX +
                            animComp->airVelocityZ * animComp->airVelocityZ;
                        const bool movingJump =
                            airSpeedSq > 0.0001f || animComp->locomotionSpeed > 0.05f;
                        if (movingJump && animComp->jumpMovingAnimation) {
                            targetAnimation = animComp->jumpMovingAnimation;
                        } else {
                            targetAnimation = animComp->jumpAnimation;
                        }
                    }
                    break;
                }
                default:
                    targetAnimation = nullptr;
                    break;
            }
        }
        
        if (animComp->currentAnimation != targetAnimation) {
            const bool wasLocomotion =
                animComp->currentAnimation == animComp->walkingAnimation ||
                animComp->currentAnimation == animComp->runningAnimation;
            const bool nowLocomotion =
                targetAnimation == animComp->walkingAnimation ||
                targetAnimation == animComp->runningAnimation;

            float preservedPhase = 0.0f;
            if (wasLocomotion && nowLocomotion && animComp->currentAnimation &&
                animComp->currentAnimation->IsLoaded()) {
                const float oldTps = animComp->currentAnimation->GetTicksPerSecond();
                const float oldDurationTicks = animComp->currentAnimation->GetDuration();
                if (oldTps > 0.0f && oldDurationTicks > 0.0f) {
                    const float oldDurationSeconds = oldDurationTicks / oldTps;
                    if (oldDurationSeconds > 0.0f) {
                        preservedPhase =
                            std::fmod(animComp->currentAnimationTime, oldDurationSeconds) /
                            oldDurationSeconds;
                    }
                }
            }

            animComp->currentAnimation = targetAnimation;
            animComp->currentAnimationTime = 0.0f;

            if (targetAnimation) {
                const bool isTurnClip =
                    targetAnimation == animComp->leftTurnAnimation ||
                    targetAnimation == animComp->rightTurnAnimation;
                // Landing/jump takeoff are one-shot; Falling and standing turns loop.
                animComp->isLooping =
                    isTurnClip ||
                    (targetAnimation == animComp->fallingAnimation) ||
                    (!animComp->landingActive &&
                     animComp->currentState !=
                         Components::AnimationComponent::AnimationState::Jumping);
                if (wasLocomotion && nowLocomotion && targetAnimation->IsLoaded()) {
                    const float newTps = targetAnimation->GetTicksPerSecond();
                    const float newDurationTicks = targetAnimation->GetDuration();
                    if (newTps > 0.0f && newDurationTicks > 0.0f) {
                        animComp->currentAnimationTime =
                            preservedPhase * (newDurationTicks / newTps);
                    }
                }
            }
        } else if (
            targetAnimation &&
            (targetAnimation == animComp->leftTurnAnimation ||
             targetAnimation == animComp->rightTurnAnimation)) {
            // Stay on the same turn clip while turning — keep looping, never rewind.
            animComp->isLooping = true;
        }

        if (airborne && animComp->currentAnimation == animComp->fallingAnimation &&
            animComp->fallingAnimation) {
            animComp->fallingTimeThisAirtime += std::max(0.0f, deltaTime);
        }

        if (animComp->currentAnimation && animComp->currentAnimation->IsLoaded() &&
            animComp->skeleton && animComp->skeleton->IsLoaded()) {
            const float ticksPerSecond = animComp->currentAnimation->GetTicksPerSecond();
            const float duration = animComp->currentAnimation->GetDuration();
            if (ticksPerSecond <= 0.0f) {
                animComp->skeleton->ResetToBindPose();
                continue;
            }

            const float durationSeconds = duration / ticksPerSecond;
            const float playbackRate = ResolvePlaybackRate(*animComp, durationSeconds);
            animComp->currentAnimationTime += deltaTime * playbackRate;
            if (animComp->isLooping) {
                if (durationSeconds > 0.0f && animComp->currentAnimationTime > durationSeconds) {
                    animComp->currentAnimationTime =
                        std::fmod(animComp->currentAnimationTime, durationSeconds);
                }
            } else {
                // Hold the last frame of one-shot clips (jump) for the rest of airtime.
                if (durationSeconds > 0.0f && animComp->currentAnimationTime > durationSeconds) {
                    animComp->currentAnimationTime = durationSeconds;
                }
            }

            const float timeInTicks = animComp->currentAnimationTime * ticksPerSecond;
            std::map<std::string, glm::mat4> animationTransforms;
            animComp->currentAnimation->GetBoneTransforms(timeInTicks, animationTransforms);
            animComp->skeleton->UpdateBoneTransforms(animationTransforms);
        } else if (animComp->skeleton && animComp->skeleton->IsLoaded()) {
            animComp->skeleton->ResetToBindPose();
        }
    }
}

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine
