#include <gtest/gtest.h>

#include "core/network/MigrationToken.h"

TEST(MigrationTokenTest, RoundTripAndVerify) {
    const std::string secret = "scale-test-secret";
    const uint64_t expiry = 4102444800ULL;
    const std::string token =
        Engine::Net::IssueMigrationToken(42, 1, 2, expiry, secret);

    Engine::Net::MigrationTokenPayload parsed;
    ASSERT_TRUE(Engine::Net::ParseMigrationToken(token, parsed));
    EXPECT_EQ(parsed.userId, 42u);
    EXPECT_EQ(parsed.sourceShardId, 1u);
    EXPECT_EQ(parsed.targetShardId, 2u);
    EXPECT_TRUE(Engine::Net::VerifyMigrationToken(parsed, secret, expiry));
    EXPECT_FALSE(Engine::Net::VerifyMigrationToken(parsed, secret, expiry + 1));
}

TEST(MigrationTokenTest, RejectsTamperedToken) {
    const std::string secret = "scale-test-secret";
    std::string token = Engine::Net::IssueMigrationToken(7, 1, 2, 9999999999ull, secret);
    token.back() = token.back() == 'a' ? 'b' : 'a';

    Engine::Net::MigrationTokenPayload parsed;
    ASSERT_TRUE(Engine::Net::ParseMigrationToken(token, parsed));
    EXPECT_FALSE(Engine::Net::VerifyMigrationToken(parsed, secret, 1));
}
