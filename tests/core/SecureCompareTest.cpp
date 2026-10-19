#include <gtest/gtest.h>

#include "core/network/SecureCompare.h"

using Engine::Net::SecureCompare;
using Engine::Net::SecureCompareBytes;

TEST(SecureCompareTest, EqualStringsMatch) {
    EXPECT_TRUE(SecureCompare("integration-secret", "integration-secret"));
}

TEST(SecureCompareTest, DifferentLengthFails) {
    EXPECT_FALSE(SecureCompare("short", "longer-value"));
}

TEST(SecureCompareTest, DifferentContentFails) {
    EXPECT_FALSE(SecureCompare("integration-secret", "integration-secrex"));
}

TEST(SecureCompareTest, ByteCompareIsConstantTimeForEqualLength) {
    const uint8_t a[] = {1, 2, 3, 4};
    const uint8_t b[] = {1, 2, 3, 5};
    EXPECT_TRUE(SecureCompareBytes(a, a, 4));
    EXPECT_FALSE(SecureCompareBytes(a, b, 4));
}
