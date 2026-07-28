#pragma once

#include "core/network/NetBuffer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

struct ReplicatedEquippedItem {
    uint16_t slot = 0;
    std::string itemId;
};

struct ReplicatedLoadout {
    std::string bodyItemId;
    std::vector<ReplicatedEquippedItem> equipment;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteString(bodyItemId);
        const uint16_t count = static_cast<uint16_t>(equipment.size());
        buffer.WriteU16(count);
        for (const ReplicatedEquippedItem& item : equipment) {
            buffer.WriteU16(item.slot);
            buffer.WriteString(item.itemId);
        }
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, ReplicatedLoadout& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        uint16_t count = 0;
        if (!copy.ReadString(out.bodyItemId) || !copy.ReadU16(count)) {
            return false;
        }
        out.equipment.clear();
        out.equipment.reserve(count);
        for (uint16_t i = 0; i < count; ++i) {
            ReplicatedEquippedItem item;
            if (!copy.ReadU16(item.slot) || !copy.ReadString(item.itemId)) {
                return false;
            }
            out.equipment.push_back(std::move(item));
        }
        return true;
    }
};

} // namespace Net
} // namespace Engine
