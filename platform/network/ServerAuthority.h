#pragma once

#include "core/network/NetworkPayloads.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Networking {

struct AuthorityValidationResult {
    bool accepted = true;
    glm::vec3 correctedPosition{0.0f};
    glm::vec3 correctedRotationDegrees{0.0f};
    std::string reason;
};

using CustomAuthorityValidator =
    std::function<AuthorityValidationResult(const Engine::Net::TransformStatePayload& proposed,
                                            const Engine::Net::TransformStatePayload& lastAuthoritative,
                                            float deltaSeconds)>;

// Server-side authority checks for movement and state application.
class ServerAuthority {
public:
    void SetMaxSpeed(float unitsPerSecond) { m_maxSpeed = std::max(unitsPerSecond, 0.0f); }
    void SetMaxVerticalDelta(float delta) { m_maxVerticalDelta = std::max(delta, 0.0f); }
    void SetCustomValidator(CustomAuthorityValidator validator) { m_customValidator = std::move(validator); }

    AuthorityValidationResult ValidateTransform(const Engine::Net::TransformStatePayload& proposed,
                                                const Engine::Net::TransformStatePayload& lastAuthoritative,
                                                float deltaSeconds) const;

    void ApplyAuthoritative(Engine::Net::UserId userId, const Engine::Net::TransformStatePayload& state);
  const Engine::Net::TransformStatePayload* GetAuthoritative(Engine::Net::UserId userId) const;

private:
    float m_maxSpeed = 25.0f;
    float m_maxVerticalDelta = 8.0f;
    CustomAuthorityValidator m_customValidator;
    std::unordered_map<Engine::Net::UserId, Engine::Net::TransformStatePayload> m_authoritativeStates;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
