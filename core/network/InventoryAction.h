#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"

#include <cstdint>

namespace Engine {
namespace Net {

enum class InventoryActionType : uint8_t {
    Use = 0,
    Drop = 1,
    Pickup = 2,
    Combine = 3,
};

struct InventoryActionCommand {
    InventoryActionType action = InventoryActionType::Use;
    uint16_t slotIndex = 0;
    uint16_t secondarySlotIndex = 0;
    uint32_t worldEntityId = 0;
};

inline NetBuffer SerializeInventoryAction(const InventoryActionCommand& command) {
    NetBuffer buffer;
    buffer.WriteU8(static_cast<uint8_t>(command.action));
    buffer.WriteU16(command.slotIndex);
    buffer.WriteU16(command.secondarySlotIndex);
    buffer.WriteU32(command.worldEntityId);
    return buffer;
}

inline bool DeserializeInventoryAction(const NetBuffer& buffer, InventoryActionCommand& out) {
    NetBuffer copy = buffer;
    copy.ResetRead();
    uint8_t action = 0;
    if (!copy.ReadU8(action) || !copy.ReadU16(out.slotIndex)) {
        return false;
    }
    out.action = static_cast<InventoryActionType>(action);
    out.secondarySlotIndex = 0;
    out.worldEntityId = 0;
    copy.ReadU16(out.secondarySlotIndex);
    copy.ReadU32(out.worldEntityId);
    return true;
}

} // namespace Net
} // namespace Engine
