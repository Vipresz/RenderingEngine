#include <gtest/gtest.h>

#include "core/math/DeterministicMath.h"

using Engine::Math::DeterministicRNG;
using Engine::Math::Vec3;

TEST(DeterministicMathTest, RngIsDeterministicForSameSeed) {
    DeterministicRNG a(12345);
    DeterministicRNG b(12345);

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a.Next(), b.Next());
    }
}

TEST(DeterministicMathTest, RngDiffersForDifferentSeeds) {
    DeterministicRNG a(1);
    DeterministicRNG b(2);
    EXPECT_NE(a.Next(), b.Next());
}

TEST(DeterministicMathTest, Vec3Operators) {
    const Vec3 a(1.0f, 2.0f, 3.0f);
    const Vec3 b(4.0f, 5.0f, 6.0f);

    const Vec3 sum = a + b;
    EXPECT_FLOAT_EQ(sum.x, 5.0f);
    EXPECT_FLOAT_EQ(sum.y, 7.0f);

    const Vec3 scaled = a * 2.0f;
    EXPECT_FLOAT_EQ(scaled.x, 2.0f);

    EXPECT_FLOAT_EQ(a.Dot(b), 32.0f);
}

TEST(DeterministicMathTest, Vec3Normalize) {
    const Vec3 v(3.0f, 0.0f, 4.0f);
    const Vec3 n = v.Normalized();
    EXPECT_NEAR(n.Length(), 1.0f, 0.0001f);
}
