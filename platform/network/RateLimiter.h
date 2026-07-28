#pragma once

#include "core/network/NetTypes.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Engine {
namespace Platform {
namespace Networking {

// Token-bucket rate limiter keyed by user id.
class RateLimiter {
public:
    RateLimiter(float tokensPerSecond, float burstCapacity);

    void SetRate(float tokensPerSecond, float burstCapacity);
    bool TryConsume(Engine::Net::UserId userId, float cost = 1.0f);

private:
    struct BucketState {
        double tokens = 0.0;
        std::chrono::steady_clock::time_point lastRefill =
            std::chrono::steady_clock::now();
    };

    void Refill(BucketState& bucket, std::chrono::steady_clock::time_point now);

    float m_tokensPerSecond = 0.0f;
    float m_burstCapacity = 0.0f;
    mutable std::mutex m_mutex;
    std::unordered_map<Engine::Net::UserId, BucketState> m_buckets;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
