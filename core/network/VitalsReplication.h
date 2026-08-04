#pragma once

#include "core/network/NetBuffer.h"

namespace Engine {
namespace Net {

struct ReplicatedVitals {
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
    float currentMana = 100.0f;
    float maxMana = 100.0f;

    NetBuffer Serialize() const {
        NetBuffer buffer;
        buffer.WriteFloat(currentHealth);
        buffer.WriteFloat(maxHealth);
        buffer.WriteFloat(currentMana);
        buffer.WriteFloat(maxMana);
        return buffer;
    }

    static bool Deserialize(const NetBuffer& buffer, ReplicatedVitals& out) {
        NetBuffer copy = buffer;
        copy.ResetRead();
        return copy.ReadFloat(out.currentHealth) && copy.ReadFloat(out.maxHealth) &&
               copy.ReadFloat(out.currentMana) && copy.ReadFloat(out.maxMana);
    }
};

} // namespace Net
} // namespace Engine
