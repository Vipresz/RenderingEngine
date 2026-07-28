#pragma once

#include "core/Types.h"
#include <glm/glm.hpp>

namespace Engine {
namespace Platform {
namespace Render {

// Game/engine hook: resolve camera collisions against world geometry (not characters).
class ICameraObstructionQuery {
public:
    virtual ~ICameraObstructionQuery() = default;

    // Closest hit along segment [from, to], excluding skipEntityId.
    // outHitDistance is distance from `from` toward `to` (0 = at pivot, segment length = unobstructed).
    virtual bool QuerySegment(const glm::vec3& from, const glm::vec3& to, EntityID skipEntityId,
                              float& outHitDistance) const = 0;
};

} // namespace Render
} // namespace Platform
} // namespace Engine
