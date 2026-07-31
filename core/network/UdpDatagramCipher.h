#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

constexpr uint16_t kUdpProtocolVersionEncrypted = 2;
constexpr size_t kUdpCipherNonceBytes = 12;
constexpr size_t kUdpCipherTagBytes = 16;

struct UdpCipherKey {
    uint8_t bytes[32]{};
};

// Derive a per-session UDP encryption key from the login session token and auth token.
UdpCipherKey DeriveUdpCipherKey(uint64_t sessionToken, const std::string& authToken);

// Encrypts plaintext into [nonce | ciphertext | tag]. Returns false on invalid input.
bool EncryptUdpPayload(const UdpCipherKey& key,
                       uint64_t sequence,
                       const uint8_t* plaintext,
                       size_t plaintextSize,
                       std::vector<uint8_t>& outEncrypted);

bool DecryptUdpPayload(const UdpCipherKey& key,
                       uint64_t sequence,
                       const uint8_t* encrypted,
                       size_t encryptedSize,
                       std::vector<uint8_t>& outPlaintext);

} // namespace Net
} // namespace Engine
