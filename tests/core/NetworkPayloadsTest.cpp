#include <gtest/gtest.h>

#include "core/network/NetworkPayloads.h"
#include "core/network/NetBuffer.h"

using Engine::Net::LoginRequestPayload;
using Engine::Net::LoginResponsePayload;
using Engine::Net::NetBuffer;
using Engine::Net::TransformStatePayload;

TEST(NetworkPayloadsTest, LoginRequestRoundTrip) {
    LoginRequestPayload original;
    original.preferredUserId = 42;
    original.displayName = "TestPlayer";
    original.authToken = "dev-secret";

    LoginRequestPayload decoded;
    ASSERT_TRUE(LoginRequestPayload::Deserialize(original.Serialize(), decoded));
    EXPECT_EQ(decoded.preferredUserId, 42u);
    EXPECT_EQ(decoded.displayName, "TestPlayer");
    EXPECT_EQ(decoded.authToken, "dev-secret");
}

TEST(NetworkPayloadsTest, LoginRequestLegacyPayloadWithoutAuthToken) {
    NetBuffer legacy;
    legacy.WriteU32(5);
    legacy.WriteString("LegacyPlayer");

    LoginRequestPayload decoded;
    ASSERT_TRUE(LoginRequestPayload::Deserialize(legacy, decoded));
    EXPECT_EQ(decoded.preferredUserId, 5u);
    EXPECT_EQ(decoded.displayName, "LegacyPlayer");
    EXPECT_TRUE(decoded.authToken.empty());
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
    original.isJumping = true;
    original.jumpAnimationActive = true;
    original.jumpVelocity = 4.5f;
    original.airVelocityX = 1.25f;
    original.airVelocityZ = -0.5f;
    original.groundLevel = 1.0f;
    original.animationState = 3;

    TransformStatePayload decoded;
    ASSERT_TRUE(TransformStatePayload::Deserialize(original.Serialize(), decoded));
    EXPECT_EQ(decoded.userId, 99u);
    EXPECT_EQ(decoded.tick, 1234u);
    EXPECT_FLOAT_EQ(decoded.position.x, 1.0f);
    EXPECT_FLOAT_EQ(decoded.position.y, 2.0f);
    EXPECT_FLOAT_EQ(decoded.position.z, 3.0f);
    EXPECT_FLOAT_EQ(decoded.rotationDegrees.x, 10.0f);
    EXPECT_TRUE(decoded.isJumping);
    EXPECT_TRUE(decoded.jumpAnimationActive);
    EXPECT_FLOAT_EQ(decoded.jumpVelocity, 4.5f);
    EXPECT_FLOAT_EQ(decoded.airVelocityX, 1.25f);
    EXPECT_FLOAT_EQ(decoded.airVelocityZ, -0.5f);
    EXPECT_FLOAT_EQ(decoded.groundLevel, 1.0f);
    EXPECT_EQ(decoded.animationState, 3u);
}

TEST(NetworkPayloadsTest, TransformStateLegacyPayloadWithoutJumpFields) {
    NetBuffer legacy;
    legacy.WriteU32(11);
    legacy.WriteU64(22);
    legacy.WriteFloat(1.0f);
    legacy.WriteFloat(2.0f);
    legacy.WriteFloat(3.0f);
    legacy.WriteFloat(0.0f);
    legacy.WriteFloat(90.0f);
    legacy.WriteFloat(0.0f);

    TransformStatePayload decoded;
    ASSERT_TRUE(TransformStatePayload::Deserialize(legacy, decoded));
    EXPECT_EQ(decoded.userId, 11u);
    EXPECT_FALSE(decoded.isJumping);
    EXPECT_FLOAT_EQ(decoded.groundLevel, 2.0f);
}
