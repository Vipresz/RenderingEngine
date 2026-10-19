#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/ReplicationTypes.h"

#include <cstdint>
#include <string>

namespace Engine {
namespace Net {

constexpr NetEntityId kWorldEntityIdBase = 0x80000000u;

inline bool IsWorldEntity(NetEntityId entityId) {
    return entityId >= kWorldEntityIdBase;
}

constexpr ReplicationComponentId kWorldDropReplicationComponent = 6;

struct ReplicatedWorldDrop {
    std::string itemId;
    uint32_t quantity = 1;
    uint64_t instanceId = 0;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteString(itemId);
        buffer.WriteU32(quantity);
        buffer.WriteU64(instanceId);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, ReplicatedWorldDrop& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadString(out.itemId) && copy.ReadU32(out.quantity) &&
               copy.ReadU64(out.instanceId);
    }
};

} // namespace Net
} // namespace Engine
