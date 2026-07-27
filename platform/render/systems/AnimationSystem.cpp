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

AnimationSystem::AnimationSystem() {
    SetPriority(50); // Run after InputSystem (0) but before RenderSystem (100)
}

void AnimationSystem::Initialize(ECS::Registry& registry) {
    LOG_INFO("AnimationSystem", "AnimationSystem initialized");
}

void AnimationSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    UpdateAnimationPlayback(deltaTime, registry);
}

void AnimationSystem::Shutdown(ECS::Registry& registry) {
    // Cleanup if needed
}

void AnimationSystem::UpdateAnimationPlayback(float deltaTime, ECS::Registry& registry) {
    auto entities = registry.View<Components::AnimationComponent>();
    
    for (auto entityID : entities) {
        auto* animComp = registry.GetComponent<Components::AnimationComponent>(entityID);
        if (!animComp) {
            continue;
        }
        
        Animation* targetAnimation = nullptr;
        
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
            case Components::AnimationComponent::AnimationState::Jumping:
                targetAnimation = animComp->jumpAnimation;
                break;
            default:
                targetAnimation = nullptr;
                break;
        }
        
        if (animComp->currentAnimation != targetAnimation) {
            animComp->currentAnimation = targetAnimation;
            animComp->currentAnimationTime = 0.0f;
            
            if (targetAnimation) {
                animComp->isLooping =
                    (animComp->currentState != Components::AnimationComponent::AnimationState::Jumping);
            }
        }

        if (animComp->currentAnimation && animComp->currentAnimation->IsLoaded() &&
            animComp->skeleton && animComp->skeleton->IsLoaded()) {
            const float ticksPerSecond = animComp->currentAnimation->GetTicksPerSecond();
            const float duration = animComp->currentAnimation->GetDuration();
            if (ticksPerSecond <= 0.0f) {
                animComp->skeleton->ResetToBindPose();
                continue;
            }

            animComp->currentAnimationTime += deltaTime;
            if (animComp->isLooping) {
                const float durationSeconds = duration / ticksPerSecond;
                if (durationSeconds > 0.0f && animComp->currentAnimationTime > durationSeconds) {
                    animComp->currentAnimationTime =
                        std::fmod(animComp->currentAnimationTime, durationSeconds);
                }
            } else {
                // Hold the last frame of one-shot clips (jump) for the rest of airtime.
                const float durationSeconds = duration / ticksPerSecond;
                if (durationSeconds > 0.0f && animComp->currentAnimationTime > durationSeconds) {
                    animComp->currentAnimationTime = durationSeconds;
                }
            }

            const float timeInTicks = animComp->currentAnimationTime * ticksPerSecond;
            std::map<std::string, glm::mat4> animationTransforms;
            animComp->currentAnimation->GetBoneTransforms(timeInTicks, animationTransforms);
            animComp->skeleton->UpdateBoneTransforms(animationTransforms);

            static int s_animDiagFrames = 0;
            if (s_animDiagFrames < 3 &&
                (animComp->currentState == Components::AnimationComponent::AnimationState::Walking ||
                 animComp->currentState == Components::AnimationComponent::AnimationState::Running)) {
                ++s_animDiagFrames;
                LOG_INFO_FMT("AnimationSystem",
                             "Applying %s clip channels=%zu bindPose=%d",
                             animComp->currentState == Components::AnimationComponent::AnimationState::Running
                                 ? "run"
                                 : "walk",
                             animationTransforms.size(),
                             animComp->skeleton->IsBindPoseActive() ? 1 : 0);
            }
        } else if (animComp->skeleton && animComp->skeleton->IsLoaded()) {
            animComp->skeleton->ResetToBindPose();
        }
    }
}

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine
