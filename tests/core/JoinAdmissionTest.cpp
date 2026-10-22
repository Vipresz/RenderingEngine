#include <gtest/gtest.h>

#include "core/network/JoinAdmission.h"

using Engine::Net::ComputeJoinStaggerDelayMs;

TEST(JoinAdmissionTest, SmallGroupsAndTheFirstPlayerAreNotStaggered) {
    EXPECT_EQ(ComputeJoinStaggerDelayMs(0, 200), 0);
    EXPECT_EQ(ComputeJoinStaggerDelayMs(10, 32), 0);
    EXPECT_EQ(ComputeJoinStaggerDelayMs(-1, 200), 0);
}

TEST(JoinAdmissionTest, DelayStaysInsideTheSpreadWindow) {
    const int playerCount = 400;
    const int maxSpreadMs = 250;
    for (int index = 1; index < 50; ++index) {
        const int delay = ComputeJoinStaggerDelayMs(index, playerCount);
        EXPECT_GE(delay, 0);
        EXPECT_LT(delay, maxSpreadMs);
    }
}

TEST(JoinAdmissionTest, SpreadIsClampedAndStable) {
    EXPECT_LT(ComputeJoinStaggerDelayMs(7, 10000), 2000);
    EXPECT_EQ(ComputeJoinStaggerDelayMs(7, 10000), ComputeJoinStaggerDelayMs(7, 10000));
    EXPECT_GE(ComputeJoinStaggerDelayMs(3, 40), 0);
}
