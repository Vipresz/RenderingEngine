#include "network/ServerAuthority.h"

#include <algorithm>
#include <cmath>

namespace Engine {
namespace Platform {
namespace Networking {

AuthorityValidationResult ServerAuthority::ValidateTransform(
    const Engine::Net::TransformStatePayload& proposed,
    const Engine::Net::TransformStatePayload& lastAuthoritative,
    float deltaSeconds) const {
    AuthorityValidationResult result;
    result.correctedPosition = proposed.position;
    result.correctedRotationDegrees = proposed.rotationDegrees;

    if (m_customValidator) {
        return m_customValidator(proposed, lastAuthoritative, deltaSeconds);
    }

    const glm::vec3 delta = proposed.position - lastAuthoritative.position;
    const float horizontalDistance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    const float maxDistance = m_maxSpeed * std::max(deltaSeconds, 1.0f / 60.0f) * 1.5f;

    if (horizontalDistance > maxDistance) {
        result.accepted = false;
        result.reason = "speed_limit";
        const glm::vec3 direction = horizontalDistance > 0.0001f
            ? glm::normalize(glm::vec3(delta.x, 0.0f, delta.z))
            : glm::vec3(0.0f);
        result.correctedPosition = lastAuthoritative.position + direction * maxDistance;
        result.correctedPosition.y = proposed.position.y;
    }

    if (std::abs(proposed.position.y - lastAuthoritative.position.y) > m_maxVerticalDelta) {
        result.accepted = false;
        result.reason = "vertical_delta";
        result.correctedPosition.y = lastAuthoritative.position.y +
            std::clamp(proposed.position.y - lastAuthoritative.position.y,
                       -m_maxVerticalDelta,
                       m_maxVerticalDelta);
    }

    return result;
}

void ServerAuthority::ApplyAuthoritative(Engine::Net::UserId userId,
                                         const Engine::Net::TransformStatePayload& state) {
    m_authoritativeStates[userId] = state;
}

const Engine::Net::TransformStatePayload* ServerAuthority::GetAuthoritative(Engine::Net::UserId userId) const {
    const auto it = m_authoritativeStates.find(userId);
    return it == m_authoritativeStates.end() ? nullptr : &it->second;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
