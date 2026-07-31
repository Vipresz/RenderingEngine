#include <gtest/gtest.h>

#include "core/network/NetworkAuth.h"
#include "core/network/SecureCompare.h"
#include "core/network/SecureSecret.h"
#include "core/network/ServerSecurityPolicy.h"
#include "core/network/CredentialStore.h"

using Engine::Net::CanAcceptLogin;
using Engine::Net::CredentialStore;
using Engine::Net::IsAuthenticationConfigured;
using Engine::Net::SecureCompareBytes;
using Engine::Net::SecureZeroString;
using Engine::Net::ServerSecurityPolicy;
using Engine::Net::VerifyAuthToken;

TEST(NetworkAuthTest, VerifyAuthTokenUsesConstantLengthDigest) {
    EXPECT_TRUE(VerifyAuthToken("integration-secret", "integration-secret"));
    EXPECT_FALSE(VerifyAuthToken("integration-secret", "integration-secrex"));
    EXPECT_FALSE(VerifyAuthToken("integration-secret", "short"));
    EXPECT_TRUE(VerifyAuthToken("", "anything"));
}

TEST(ServerSecurityPolicyTest, EnforceAuthenticationRequiresConfiguredGate) {
    CredentialStore store;
    ServerSecurityPolicy policy;
    policy.enforceAuthentication = true;

    EXPECT_FALSE(IsAuthenticationConfigured("", store));
    EXPECT_FALSE(CanAcceptLogin(policy, "", store));

    policy.enforceAuthentication = false;
    EXPECT_TRUE(CanAcceptLogin(policy, "", store));

    EXPECT_TRUE(CanAcceptLogin(policy, "token", store));
    store.RegisterUser("alice", "secret");
    EXPECT_TRUE(CanAcceptLogin(policy, "", store));
}

TEST(SecureSecretTest, SecureZeroStringClearsString) {
    std::string secret = "super-secret-password";
    SecureZeroString(secret);
    EXPECT_TRUE(secret.empty());
}

TEST(SecureCompareTest, DigestAuthAvoidsLengthLeakForEqualTokens) {
    const std::string token = "0123456789abcdef0123456789abcdef";
    EXPECT_TRUE(VerifyAuthToken(token, token));
    EXPECT_FALSE(VerifyAuthToken(token, token.substr(0, token.size() - 1)));
}
