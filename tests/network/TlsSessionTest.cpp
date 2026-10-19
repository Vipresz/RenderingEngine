#include <gtest/gtest.h>

#include "network/TlsSession.h"
#include "TestRuntime.h"

#include <filesystem>

using Engine::Platform::Networking::TlsConfig;
using Engine::Platform::Networking::TlsSession;

namespace {

std::filesystem::path FixtureTlsPath(const char* fileName) {
    const std::filesystem::path runtimePath =
        Engine::Tests::GetExecutableDirectory() / "fixtures" / "tls" / fileName;
    if (std::filesystem::exists(runtimePath)) {
        return runtimePath;
    }
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures" / "tls" / fileName;
}

} // namespace

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

TEST(TlsSessionTest, AllowsInsecureClientWhenSkipVerifyEnabled) {
    TlsSession session;
    TlsConfig config;
    config.enabled = true;
    config.insecureSkipVerify = true;
    EXPECT_TRUE(session.InitializeClient(1, config));
    EXPECT_TRUE(session.IsActive());
}

TEST(TlsSessionTest, RejectsServerWithoutCertificatePaths) {
    TlsSession session;
    TlsConfig config;
    config.enabled = true;
    EXPECT_FALSE(session.InitializeServer(1, config));
    EXPECT_FALSE(session.IsActive());
}

TEST(TlsSessionTest, LoadsFixtureServerCertificateAndKey) {
    ASSERT_TRUE(Engine::Tests::SetWorkingDirectoryToExecutable());
    const std::filesystem::path certPath = FixtureTlsPath("test_server.crt");
    const std::filesystem::path keyPath = FixtureTlsPath("test_server.key");
    ASSERT_TRUE(std::filesystem::exists(certPath)) << certPath.string();
    ASSERT_TRUE(std::filesystem::exists(keyPath)) << keyPath.string();

    TlsSession session;
    TlsConfig config;
    config.enabled = true;
    config.certificatePath = certPath.string();
    config.privateKeyPath = keyPath.string();
    EXPECT_TRUE(session.InitializeServer(1, config));
    EXPECT_TRUE(session.IsActive());
}
#endif
