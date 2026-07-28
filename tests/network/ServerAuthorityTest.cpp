#include <gtest/gtest.h>

#include "network/ServerAuthority.h"

using Engine::Net::TransformStatePayload;
using Engine::Platform::Networking::ServerAuthority;

TEST(ServerAuthorityTest, AcceptsSmallMovement) {
    ServerAuthority authority;
    authority.SetMaxSpeed(10.0f);

    TransformStatePayload last;
    last.position = glm::vec3(0.0f, 0.0f, 0.0f);

    TransformStatePayload proposed = last;
    proposed.position = glm::vec3(0.1f, 0.0f, 0.1f);

    const auto result = authority.ValidateTransform(proposed, last, 1.0f / 60.0f);
    EXPECT_TRUE(result.accepted);
}

TEST(ServerAuthorityTest, RejectsTeleport) {
    ServerAuthority authority;
    authority.SetMaxSpeed(5.0f);

    TransformStatePayload last;
    last.position = glm::vec3(0.0f, 0.0f, 0.0f);

    TransformStatePayload proposed = last;
    proposed.position = glm::vec3(100.0f, 0.0f, 0.0f);

    const auto result = authority.ValidateTransform(proposed, last, 1.0f / 60.0f);
    EXPECT_FALSE(result.accepted);
    EXPECT_LT(glm::length(result.correctedPosition - last.position), 100.0f);
}

TEST(ServerAuthorityTest, StoresAuthoritativeState) {
    ServerAuthority authority;
    TransformStatePayload state;
    state.userId = 42;
    state.position = glm::vec3(1.0f, 2.0f, 3.0f);

    authority.ApplyAuthoritative(42, state);
    const TransformStatePayload* stored = authority.GetAuthoritative(42);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->position.y, 2.0f);
}
