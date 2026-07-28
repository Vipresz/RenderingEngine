#include <gtest/gtest.h>

#include "network/RateLimiter.h"

using Engine::Platform::Networking::RateLimiter;

TEST(RateLimiterTest, FirstConsumeSucceedsWithoutWaiting) {
    RateLimiter limiter(10.0f, 1.0f);
    EXPECT_TRUE(limiter.TryConsume(42));
}

TEST(RateLimiterTest, AllowsBurstThenThrottles) {
    RateLimiter limiter(10.0f, 2.0f);
    EXPECT_TRUE(limiter.TryConsume(1));
    EXPECT_TRUE(limiter.TryConsume(1));
    EXPECT_FALSE(limiter.TryConsume(1));
}

TEST(RateLimiterTest, SeparateBucketsPerUser) {
    RateLimiter limiter(10.0f, 1.0f);
    EXPECT_TRUE(limiter.TryConsume(1));
    EXPECT_TRUE(limiter.TryConsume(2));
    EXPECT_FALSE(limiter.TryConsume(1));
    EXPECT_FALSE(limiter.TryConsume(2));
}
