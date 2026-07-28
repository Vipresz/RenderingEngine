#pragma once

#include "core/network/MovementCommand.h"

#include <array>
#include <cstddef>
#include <functional>

namespace Engine {
namespace Net {

class InputBuffer {
public:
    static constexpr size_t kCapacity = 256;

    void Clear() {
        m_head = 0;
        m_size = 0;
    }

    void Push(MovementCommand command) {
        if (m_size < kCapacity) {
            m_buffer[(m_head + m_size) % kCapacity] = command;
            ++m_size;
            return;
        }

        m_buffer[m_head] = command;
        m_head = (m_head + 1) % kCapacity;
    }

    size_t Size() const { return m_size; }

    const MovementCommand* GetAt(size_t index) const {
        if (index >= m_size) {
            return nullptr;
        }
        return &m_buffer[(m_head + index) % kCapacity];
    }

    void ForEachAfter(uint64_t tick, const std::function<void(const MovementCommand&)>& fn) const {
        for (size_t i = 0; i < m_size; ++i) {
            const MovementCommand& command = m_buffer[(m_head + i) % kCapacity];
            if (command.tick > tick) {
                fn(command);
            }
        }
    }

private:
    std::array<MovementCommand, kCapacity> m_buffer{};
    size_t m_head = 0;
    size_t m_size = 0;
};

} // namespace Net
} // namespace Engine
