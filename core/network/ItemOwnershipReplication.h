#pragma once

#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

struct ReplicatedOwnedItem {
    std::string itemId;
    uint32_t quantity = 1;
    uint64_t instanceId = 0;
};

struct ReplicatedItemOwnership {
    UserId ownerUserId = kInvalidUserId;
    std::vector<ReplicatedOwnedItem> items;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteU32(ownerUserId);
        const uint16_t count = static_cast<uint16_t>(items.size());
        buffer.WriteU16(count);
        for (const ReplicatedOwnedItem& item : items) {
            buffer.WriteString(item.itemId);
            buffer.WriteU32(item.quantity);
            buffer.WriteU64(item.instanceId);
        }
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, ReplicatedItemOwnership& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        uint16_t count = 0;
        if (!copy.ReadU32(out.ownerUserId) || !copy.ReadU16(count)) {
            return false;
        }
        out.items.clear();
        out.items.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            ReplicatedOwnedItem item;
            if (!copy.ReadString(item.itemId) || !copy.ReadU32(item.quantity) ||
                !copy.ReadU64(item.instanceId)) {
                return false;
            }
            out.items.push_back(std::move(item));
        }
        return true;
    }
};

ECS::PlayerItemOwnershipComponent ToOwnershipComponent(const ReplicatedItemOwnership& replicated);
ReplicatedItemOwnership FromOwnershipComponent(const ECS::PlayerItemOwnershipComponent& component);

} // namespace Net
} // namespace Engine
