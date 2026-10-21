#include <gtest/gtest.h>

#include "core/time/Timestep.h"

using Engine::Time::FixedTimestep;
using Engine::MAX_TICKS_PER_FRAME;

TEST(FixedTimestepTest, AccumulatesUntilAFullTick) {
    FixedTimestep step;
    EXPECT_EQ(step.Update(0.0f), 0u);
    EXPECT_EQ(step.GetTickCount(), 0u);

    const float halfTick = step.GetFixedDeltaTime() * 0.5f;
    EXPECT_EQ(step.Update(halfTick), 0u);
    EXPECT_NEAR(step.GetInterpolationAlpha(), 0.5f, 1e-4f);

    EXPECT_EQ(step.Update(halfTick), 1u);
    EXPECT_EQ(step.GetTickCount(), 1u);
    EXPECT_NEAR(step.GetAccumulator(), 0.0f, 1e-5f);
}

TEST(FixedTimestepTest, ConsumesSeveralTicksFromOneFrame) {
    FixedTimestep step;
    EXPECT_EQ(step.Update(step.GetFixedDeltaTime() * 3.0f), 3u);
    EXPECT_EQ(step.GetTickCount(), 3u);
}

TEST(FixedTimestepTest, CapsTicksAndKeepsTheRemainder) {
    FixedTimestep step(0.1f);
    const uint32_t ticks = step.Update(1.0f);
    EXPECT_EQ(ticks, MAX_TICKS_PER_FRAME);
    EXPECT_NEAR(step.GetAccumulator(), 1.0f - 0.1f * MAX_TICKS_PER_FRAME, 1e-4f);
    EXPECT_GT(step.GetInterpolationAlpha(), 1.0f);
}

TEST(FixedTimestepTest, ResetTickCountLeavesTheAccumulator) {
    FixedTimestep step;
    step.Update(step.GetFixedDeltaTime() * 2.5f);
    step.ResetTickCount();
    EXPECT_EQ(step.GetTickCount(), 0u);
    EXPECT_GT(step.GetAccumulator(), 0.0f);
}
