#include <gtest/gtest.h>

#include "core/network/UdpDatagramCipher.h"

#include <cstring>

using Engine::Net::DecryptUdpPayload;
using Engine::Net::DeriveUdpCipherKey;
using Engine::Net::EncryptUdpPayload;
using Engine::Net::kUdpCipherNonceBytes;
using Engine::Net::kUdpCipherTagBytes;

TEST(UdpDatagramCipherTest, RoundTripEncryptsAndDecryptsPayload) {
    const Engine::Net::UdpCipherKey key = DeriveUdpCipherKey(0x123456789ABCDEF0ull, "integration-secret");
    const uint8_t plaintext[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    std::vector<uint8_t> encrypted;
    ASSERT_TRUE(EncryptUdpPayload(key, 42, plaintext, sizeof(plaintext), encrypted));
    EXPECT_GE(encrypted.size(), kUdpCipherNonceBytes + sizeof(plaintext) + kUdpCipherTagBytes);
    EXPECT_NE(0, std::memcmp(encrypted.data() + kUdpCipherNonceBytes, plaintext, sizeof(plaintext)));

    std::vector<uint8_t> decrypted;
    ASSERT_TRUE(DecryptUdpPayload(key, 42, encrypted.data(), encrypted.size(), decrypted));
    ASSERT_EQ(decrypted.size(), sizeof(plaintext));
    EXPECT_EQ(0, std::memcmp(decrypted.data(), plaintext, sizeof(plaintext)));
}

TEST(UdpDatagramCipherTest, TamperedCiphertextFailsAuthentication) {
    const Engine::Net::UdpCipherKey key = DeriveUdpCipherKey(99, "secret");
    const uint8_t plaintext[] = {0xAA, 0xBB};

    std::vector<uint8_t> encrypted;
    ASSERT_TRUE(EncryptUdpPayload(key, 7, plaintext, sizeof(plaintext), encrypted));
    encrypted.back() ^= 0x01;

    std::vector<uint8_t> decrypted;
    EXPECT_FALSE(DecryptUdpPayload(key, 7, encrypted.data(), encrypted.size(), decrypted));
}

TEST(UdpDatagramCipherTest, DifferentSessionTokensDeriveDifferentKeys) {
    const auto keyA = DeriveUdpCipherKey(1, "shared");
    const auto keyB = DeriveUdpCipherKey(2, "shared");
    EXPECT_NE(0, std::memcmp(keyA.bytes, keyB.bytes, sizeof(keyA.bytes)));
}
