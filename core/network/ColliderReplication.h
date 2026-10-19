#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/ReplicationTypes.h"
#include "physics/components/ColliderComponent.h"

namespace Engine {
namespace Net {

constexpr ReplicationComponentId kColliderReplicationComponent = 7;

inline NetBuffer SerializeColliderReplication(
    const Engine::Physics::Components::ColliderComponent& collider) {
    NetBuffer buffer;
    buffer.WriteFloat(collider.localBounds.min.x);
    buffer.WriteFloat(collider.localBounds.min.y);
    buffer.WriteFloat(collider.localBounds.min.z);
    buffer.WriteFloat(collider.localBounds.max.x);
    buffer.WriteFloat(collider.localBounds.max.y);
    buffer.WriteFloat(collider.localBounds.max.z);
    buffer.WriteU8(collider.isStatic ? 1 : 0);
    buffer.WriteU8(collider.useGravity ? 1 : 0);
    buffer.WriteFloat(collider.velocity.x);
    buffer.WriteFloat(collider.velocity.y);
    buffer.WriteFloat(collider.velocity.z);
    return buffer;
}

inline bool DeserializeColliderReplication(const NetBuffer& buffer,
                                           Engine::Physics::Components::ColliderComponent& out) {
    NetBuffer copy = buffer;
    copy.ResetRead();
    uint8_t isStatic = 0;
    uint8_t useGravity = 0;
    if (!copy.ReadFloat(out.localBounds.min.x) || !copy.ReadFloat(out.localBounds.min.y) ||
        !copy.ReadFloat(out.localBounds.min.z) || !copy.ReadFloat(out.localBounds.max.x) ||
        !copy.ReadFloat(out.localBounds.max.y) || !copy.ReadFloat(out.localBounds.max.z) ||
        !copy.ReadU8(isStatic) || !copy.ReadU8(useGravity) || !copy.ReadFloat(out.velocity.x) ||
        !copy.ReadFloat(out.velocity.y) || !copy.ReadFloat(out.velocity.z)) {
        return false;
    }
    out.isStatic = isStatic != 0;
    out.useGravity = useGravity != 0;
    return true;
}

} // namespace Net
} // namespace Engine
