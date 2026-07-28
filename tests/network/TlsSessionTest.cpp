#include <gtest/gtest.h>

#include "network/TlsSession.h"

using Engine::Platform::Networking::TlsConfig;
using Engine::Platform::Networking::TlsSession;

TEST(TlsSessionTest, DisabledConfigDoesNotInitialize) {
    TlsSession session;
    TlsConfig config;
    config.enabled = false;
    EXPECT_FALSE(session.InitializeClient(1, config));
    EXPECT_FALSE(session.InitializeServer(1, config));
    EXPECT_FALSE(session.IsActive());
}

#if defined(ENGINE_ENABLE_TLS)
TEST(TlsSessionTest, RejectsMissingCertificatesWhenVerificationRequired) {
    TlsSession session;
    TlsConfig config;
    config.enabled = true;
    config.insecureSkipVerify = false;
    EXPECT_FALSE(session.InitializeClient(1, config));
}
#endif
