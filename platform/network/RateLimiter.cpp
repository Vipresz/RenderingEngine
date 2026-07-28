#include "network/RateLimiter.h"

#include <algorithm>

namespace Engine {
namespace Platform {
namespace Networking {

RateLimiter::RateLimiter(float tokensPerSecond, float burstCapacity) {
    SetRate(tokensPerSecond, burstCapacity);
}

void RateLimiter::SetRate(float tokensPerSecond, float burstCapacity) {
    m_tokensPerSecond = std::max(tokensPerSecond, 0.0f);
    m_burstCapacity = std::max(burstCapacity, 1.0f);
}

void RateLimiter::Refill(BucketState& bucket, std::chrono::steady_clock::time_point now) {
    const double elapsed =
        std::chrono::duration<double>(now - bucket.lastRefill).count();
    if (elapsed <= 0.0) {
        return;
    }
    bucket.tokens = std::min(static_cast<double>(m_burstCapacity),
                             bucket.tokens + elapsed * static_cast<double>(m_tokensPerSecond));
    bucket.lastRefill = now;
}

bool RateLimiter::TryConsume(Engine::Net::UserId userId, float cost) {
    if (m_tokensPerSecond <= 0.0f || cost <= 0.0f) {
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto now = std::chrono::steady_clock::now();
    auto [it, inserted] = m_buckets.try_emplace(userId);
    BucketState& bucket = it->second;
    if (inserted) {
        bucket.tokens = static_cast<double>(m_burstCapacity);
        bucket.lastRefill = now;
    } else {
        Refill(bucket, now);
    }

    if (bucket.tokens < static_cast<double>(cost)) {
        return false;
    }

    bucket.tokens -= static_cast<double>(cost);
    return true;
}

} // namespace Networking
} // namespace Platform
} // namespace Engine
