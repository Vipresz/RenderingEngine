#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"

#include <cstdint>

namespace Engine {
namespace Net {

enum class InventoryActionType : uint8_t {
    Use = 0,
    Drop = 1,
};

struct InventoryActionCommand {
    InventoryActionType action = InventoryActionType::Use;
    uint16_t slotIndex = 0;
};

inline NetBuffer SerializeInventoryAction(const InventoryActionCommand& command) {
    NetBuffer buffer;
    buffer.WriteU8(static_cast<uint8_t>(command.action));
    buffer.WriteU16(command.slotIndex);
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
    return true;
}

} // namespace Net
} // namespace Engine
