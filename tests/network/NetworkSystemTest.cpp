#include <gtest/gtest.h>

#include "network/systems/NetworkSystem.h"

using Engine::Platform::Networking::Systems::NetworkSystem;

TEST(NetworkSystemTest, HasExpectedPriority) {
    NetworkSystem system;
    EXPECT_EQ(system.GetPriority(), 55);
}

TEST(NetworkSystemTest, StartsDisconnected) {
    NetworkSystem system;
    EXPECT_FALSE(system.IsLoggedIn());
}
