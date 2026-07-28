#include <gtest/gtest.h>

#include "core/network/PasswordHasher.h"

using Engine::Net::PasswordHasher;

TEST(PasswordHasherTest, HashAndVerifyRoundTrip) {
    const auto record = PasswordHasher::HashPassword("test-password");
    EXPECT_TRUE(PasswordHasher::VerifyPassword("test-password", record));
    EXPECT_FALSE(PasswordHasher::VerifyPassword("wrong-password", record));
}
