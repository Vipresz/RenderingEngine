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
