#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

namespace Engine {
namespace Net {

// Spreads concurrent client logins over a short window to reduce thundering-herd
// contention on a single shard. Safe for production connect paths and stress tests.
inline int ComputeJoinStaggerDelayMs(int playerIndex, int playerCount) {
    if (playerCount <= 32 || playerIndex <= 0) {
        return 0;
    }

    const int maxSpreadMs = std::clamp(playerCount / 4, 250, 2000);
    const uint32_t hash =
        static_cast<uint32_t>(playerIndex) * 2654435761u + static_cast<uint32_t>(playerCount);
    return static_cast<int>(hash % static_cast<uint32_t>(maxSpreadMs));
}

inline void SleepJoinStagger(int playerIndex, int playerCount) {
    const int delayMs = ComputeJoinStaggerDelayMs(playerIndex, playerCount);
    if (delayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}

} // namespace Net
} // namespace Engine
