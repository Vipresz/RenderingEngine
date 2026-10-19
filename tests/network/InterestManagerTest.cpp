#include <gtest/gtest.h>

#include "network/InterestManager.h"

using Engine::Net::UserId;
using Engine::Platform::Networking::InterestManager;

TEST(InterestManagerTest, NearbyUsersWithinRadius) {
    InterestManager interest(10.0f, 50.0f);
    interest.UpdateEntity(1, glm::vec3(0.0f, 0.0f, 0.0f));
    interest.UpdateEntity(2, glm::vec3(20.0f, 0.0f, 0.0f));
    interest.UpdateEntity(3, glm::vec3(200.0f, 0.0f, 0.0f));

    const auto nearby = interest.QueryNearbyUsers(glm::vec3(0.0f, 0.0f, 0.0f), 50.0f);
    EXPECT_EQ(nearby.size(), 2u);
}

TEST(InterestManagerTest, InterestedObserversExcludeSource) {
    InterestManager interest(10.0f, 100.0f);
    interest.UpdateEntity(1, glm::vec3(0.0f, 0.0f, 0.0f));
    interest.UpdateEntity(2, glm::vec3(15.0f, 0.0f, 0.0f));

    const auto observers = interest.GetInterestedObservers(1, glm::vec3(0.0f, 0.0f, 0.0f));
    ASSERT_EQ(observers.size(), 1u);
    EXPECT_EQ(observers[0], 2u);
}

TEST(InterestManagerTest, RemoveEntityClearsFromQueries) {
    InterestManager interest(10.0f, 100.0f);
    interest.UpdateEntity(1, glm::vec3(0.0f, 0.0f, 0.0f));
    interest.RemoveEntity(1);

    const auto nearby = interest.QueryNearbyUsers(glm::vec3(0.0f, 0.0f, 0.0f), 100.0f);
    EXPECT_TRUE(nearby.empty());
}

TEST(InterestManagerTest, CellDensityTracksCrowding) {
    InterestManager interest(32.0f, 128.0f);
    for (Engine::Net::UserId userId = 1; userId <= 40; ++userId) {
        interest.UpdateEntity(userId, glm::vec3(0.0f, 0.0f, 0.0f));
    }

    EXPECT_EQ(interest.CountUsersInCell(glm::vec3(0.0f, 0.0f, 0.0f)), 40u);
    EXPECT_EQ(interest.GetMaxCellDensity(), 40u);
    EXPECT_EQ(interest.CountUsersInCell(glm::vec3(100.0f, 0.0f, 0.0f)), 0u);
}

TEST(InterestManagerTest, SetCellSizeRebuildsSpatialIndex) {
    InterestManager interest(32.0f, 128.0f);
    interest.UpdateEntity(1, glm::vec3(0.0f, 0.0f, 0.0f));
    interest.UpdateEntity(2, glm::vec3(40.0f, 0.0f, 0.0f));

    EXPECT_EQ(interest.GetMaxCellDensity(), 1u);

    interest.SetCellSize(64.0f);
    EXPECT_EQ(interest.GetMaxCellDensity(), 2u);
    EXPECT_EQ(interest.CountUsersInCell(glm::vec3(0.0f, 0.0f, 0.0f)), 2u);
}
