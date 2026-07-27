#include <gtest/gtest.h>

#include "core/network/NetworkPayloads.h"

using Engine::Net::LoginRequestPayload;
using Engine::Net::LoginResponsePayload;
using Engine::Net::TransformStatePayload;

TEST(NetworkPayloadsTest, LoginRequestRoundTrip) {
    LoginRequestPayload original;
    original.preferredUserId = 42;
    original.displayName = "TestPlayer";

    LoginRequestPayload decoded;
    ASSERT_TRUE(LoginRequestPayload::Deserialize(original.Serialize(), decoded));
    EXPECT_EQ(decoded.preferredUserId, 42u);
    EXPECT_EQ(decoded.displayName, "TestPlayer");
}

TEST(NetworkPayloadsTest, LoginResponseRoundTrip) {
    LoginResponsePayload original;
    original.success = true;
    original.assignedUserId = 7;
    original.message = "ok";
    original.udpSessionToken = 0x12345678ABCDEF00ull;

    LoginResponsePayload decoded;
    ASSERT_TRUE(LoginResponsePayload::Deserialize(original.Serialize(), decoded));
    EXPECT_TRUE(decoded.success);
    EXPECT_EQ(decoded.assignedUserId, 7u);
    EXPECT_EQ(decoded.message, "ok");
    EXPECT_EQ(decoded.udpSessionToken, 0x12345678ABCDEF00ull);
}

TEST(NetworkPayloadsTest, TransformStateRoundTrip) {
    TransformStatePayload original;
    original.userId = 99;
    original.tick = 1234;
    original.position = glm::vec3(1.0f, 2.0f, 3.0f);
    original.rotationDegrees = glm::vec3(10.0f, 20.0f, 30.0f);

    TransformStatePayload decoded;
    ASSERT_TRUE(TransformStatePayload::Deserialize(original.Serialize(), decoded));
    EXPECT_EQ(decoded.userId, 99u);
    EXPECT_EQ(decoded.tick, 1234u);
    EXPECT_FLOAT_EQ(decoded.position.x, 1.0f);
    EXPECT_FLOAT_EQ(decoded.position.y, 2.0f);
    EXPECT_FLOAT_EQ(decoded.position.z, 3.0f);
    EXPECT_FLOAT_EQ(decoded.rotationDegrees.x, 10.0f);
}
