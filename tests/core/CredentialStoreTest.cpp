#include <gtest/gtest.h>

#include "core/network/CredentialStore.h"

#include <filesystem>

using Engine::Net::CredentialStore;

TEST(CredentialStoreTest, RegisterAndVerifyUser) {
    CredentialStore store;
    ASSERT_TRUE(store.RegisterUser("alice", "secret123"));
    EXPECT_TRUE(store.RequiresCredentials());
    EXPECT_TRUE(store.Verify("alice", "secret123"));
    EXPECT_FALSE(store.Verify("alice", "wrong"));
    EXPECT_FALSE(store.Verify("bob", "secret123"));
}

TEST(CredentialStoreTest, SaveAndLoadFileRoundTrip) {
    CredentialStore store;
    ASSERT_TRUE(store.RegisterUser("bob", "pw"));
    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "renderingengine_accounts_test.txt";
    ASSERT_TRUE(store.SaveToFile(tempPath.string()));

    CredentialStore loaded;
    ASSERT_TRUE(loaded.LoadFromFile(tempPath.string()));
    EXPECT_TRUE(loaded.RequiresCredentials());
    EXPECT_TRUE(loaded.Verify("bob", "pw"));
    std::filesystem::remove(tempPath);
}
