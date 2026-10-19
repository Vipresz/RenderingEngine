#pragma once

#include "core/System.h"
#include "core/ecs/Registry.h"
#include "render/components/AnimationComponent.h"

#include <glm/mat4x4.hpp>
#include <map>
#include <string>

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

/// Advances clip time and writes each skeleton's bone palette.
/// Walk and run playback tracks locomotion speed. Jump, landing, and skills are one-shot.
/// A skill replaces upper-body channels, or the whole pose when skillFullBody is set.
class AnimationSystem : public ECS::ISystem {
public:
    AnimationSystem();
    
    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;
    
private:
    void UpdateAnimationPlayback(float deltaTime, ECS::Registry& registry);

    std::map<std::string, glm::mat4> m_locomotionPose;
    std::map<std::string, glm::mat4> m_skillPose;
};

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine


